#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 by Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - Hint System
 *   
 *   This module provides a hint system framework.  Games can use this
 *   framework to define context-sensitive hints for players.
 *   
 *   This module depends on the menus module to display the user interface.
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   We refer to some properties defined primarily in score.t - that's an
 *   optional module, though, so make sure the compiler has heard of these. 
 */
property scoreCount;


/* ------------------------------------------------------------------------ */
/*
 *   A basic hint menu object.  This is an abstract base class that
 *   encapsulates some behavior common to different hint menu classes.  
 */
class HintMenuObject: object
    /*
     *   The topic order.  When we're about to show a list of open topics,
     *   we'll sort the list in ascending order of this property, then in
     *   ascending order of title.  By default, we set this order value to
     *   1000; if individual goals don't override this, then they'll
     *   simply be sorted lexically by topic name.  This can be used if
     *   there's some basis other than alphabetical order for sorting the
     *   list.  
     */
    topicOrder = 1000

    /*
     *   Compare this goal to another, for the purposes of sorting a list
     *   of topics.  Returns a positive number if this goal sorts after
     *   the other one, a negative number if this goal sorts before the
     *   other one, 0 if the relative order is arbitrary.
     *   
     *   By default, we'll sort by topicOrder if the topicOrder values are
     *   different, otherwise alphabetically by title.  
     */
    compareForTopicSort(other)
    {
        /* if the topicOrder values are different, sort by topicOrder */
        if (topicOrder != other.topicOrder)
            return topicOrder - other.topicOrder;

        /* the topicOrder values are the same, so sort by title */
        if (title > other.title)
            return 1;
        else if (title < other.title)
            return -1;
        else
            return 0;
    }
;

/*
 *   A Goal represents an open task: something that the player is trying
 *   to achieve.  A Goal is an abstract object, not part of the simulated
 *   world of the game.
 *   
 *   Each goal is associated with a hint topic (usually shown as a
 *   question, such as "How do I get past the guard?") and an ordered list
 *   of hints.  The hints are usually ordered from most general to most
 *   specific.  The idea is to let the player control how big a hint they
 *   get; we start with a small nudge and work towards giving away the
 *   puzzle completely, so the player can stop as soon as they see
 *   something that helps.
 *   
 *   At any given time, a goal can be in one of three states:
 *   
 *   - Open: this means that the player is (or ought to be) aware of the
 *   goal, but the goal hasn't yet been achieved.  Determining this
 *   awareness is up to the goal.  In some cases, a goal is opened as soon
 *   as the player has seen a particular object or entered a particular
 *   area; in other cases, a goal might be opened by a scripted event,
 *   such as a speech by an NPC telling the player they have to accomplish
 *   something.  A goal could even be opened by viewing a hint for another
 *   goal, because that hint could explain a gating goal that the player
 *   might not otherwise been able to know about.
 *   
 *   - Undiscovered: this means that the player doesn't yet have any
 *   reason to know about the goal.
 *   
 *   - Closed: this means that the player has accomplished the goal, or in
 *   some cases that the goal has become irrelevant. 
 *   
 *   The hint system only shows goals that are Open.  We don't show Closed
 *   goals because the player presumably has no need of them any longer;
 *   we don't show Undiscovered goals to avoid giving away developments
 *   later in the game before they become relevant.  
 */
enum OpenGoal, ClosedGoal, UndiscoveredGoal;
class Goal: MenuTopicItem, HintMenuObject
    /*
     *   The topic question associated with the goal.  The hint system
     *   shows a list of the topics for the goals that are currently open,
     *   so that the player can decide what area they want help on.  
     */
    title = ''

    /*
     *   Our parent menu - this is usually a HintMenu object.  In very
     *   simple hint systems, this could simply be a top-level hint menu
     *   container; more typically, the hint system will be structured
     *   into a menu tree that organizes the hint topics into several
     *   different submenus, for easier navigatino.  
     */
    location = nil

    /*
     *   The list of hints for this topic.  This should be ordered from
     *   most general to most specific; we offer the hints in the order
     *   they appear in this list, so the earlier hints should give away
     *   as little as possible, while the later hints should get
     *   progressively closer to just outright giving away the answer.
     *   
     *   Each entry in the list can be a simple (single-quoted) string, or
     *   it can be a Hint object.  In most cases, a string will do.  A
     *   Hint object is only needed when displaying the hint has some side
     *   effect, such as opening a new Goal.  
     */
    menuContents = []

    /*
     *   An optional object that, when seen by the player character, opens
     *   this goal.  It's often convenient to declare a goal open as soon
     *   as the player enters a particular area or has encountered a
     *   particular object.  For such cases, simply set this property to
     *   the room or object that opens the goal, and we'll automatically
     *   mark the goal as Open the next time the player asks for a hint
     *   after seeing the referenced object.  
     */
    openWhenSeen = nil

    /*
     *   An option object that, when seen by the player character, closes
     *   this goal.  Many goals will be things like "how do I find the
     *   X?", in which case it's nice to close the goal when the X is
     *   found. 
     */
    closeWhenSeen = nil

    /* 
     *   this is like openWhenSeen, but opens the topic when the given
     *   object is described (with EXAMINE) 
     */
    openWhenDescribed = nil

    /* close the goal when the given object is described */
    closeWhenDescribed = nil

    /*
     *   An optional Achievement object that opens this goal.  This goal
     *   will be opened automatically once the goal is achieved, if the
     *   goal was previously undiscovered.  This makes it easy to set up a
     *   hint topic that becomes available after a particular puzzle is
     *   solved, which is useful when a new puzzle only becomes known to
     *   the player after a gating puzzle has been solved.  
     */
    openWhenAchieved = nil

    /*
     *   An optional Achievement object that closes this goal.  Once the
     *   achievement is completed, this goal's state will automatically be
     *   set to Closed.  This makes it easy to associate the goal with a
     *   puzzle: once the puzzle is solved, there's no need to show hints
     *   for the goal any more.  
     */
    closeWhenAchieved = nil

    /*
     *   An optional Topic or Thing that opens this goal when the object
     *   becomes "known" to the player character.  This will open the goal
     *   as soon as gPlayerChar.knowsAbout(openWhenKnown) returns true.
     *   This makes it easy to open a goal as soon as the player comes
     *   across some information in the game.  
     */
    openWhenKnown = nil

    /* an optional Topic or Thing that closes this goal when known */
    closeWhenKnown = nil

    /*
     *   An optional <.reveal> tag name that opens this goal.  If this is
     *   set to a non-nil string, we'll automatically open this goal when
     *   the tag has been revealed via <.reveal> (or gReveal()). 
     */
    openWhenRevealed = nil

    /* an optional <.reveal> tag that closes this goal when revealed */
    closeWhenRevealed = nil

    /*
     *   An optional arbitrary check that opens the goal.  If this returns
     *   true, we'll open the goal.  This check is made in addition to the
     *   other checks (openWhenSeen, openWhenDescribed, etc).  This can be
     *   used for any custom check that doesn't fit into one of the
     *   standard openWhenXxx properties.  
     */
    openWhenTrue = nil

    /* an optional general-purpose check that closes the goal */
    closeWhenTrue = nil

    /*
     *   Determine if there's any condition that should open this goal.
     *   This checks openWhenSeen, openWhenDescribed, and all of the other
     *   openWhenXxx conditions; if any of these return true, then we'll
     *   return true.
     *   
     *   Note that this should generally NOT be overridden in individual
     *   instances; normally, instances would define openWhenTrue instead.
     *   However, some games might find that they use the same special
     *   condition over and over in many goals, often enough to warrant
     *   adding a new openWhenXxx property to Goal.  In these cases, you
     *   can use 'modify Goal' to override openWhen to add the new
     *   condition: simply define openWhen as (inherited || newCondition),
     *   where 'newCondition' is the new special condition you want to
     *   add.  
     */
    openWhen = (
        (openWhenSeen != nil && gPlayerChar.hasSeen(openWhenSeen))
        || (openWhenDescribed != nil && openWhenDescribed.described)
        || (openWhenAchieved != nil && openWhenAchieved.scoreCount != 0)
        || (openWhenKnown != nil && gPlayerChar.knowsAbout(openWhenKnown))
        || (openWhenRevealed != nil && gRevealed(openWhenRevealed))
        || openWhenTrue)

    /*
     *   Determine if there's any condition that should close this goal.
     *   We'll check closeWhenSeen, closeWhenDescribed, and all of the
     *   other closeWhenXxx conditions; if any of these return true, then
     *   we'll return true. 
     */
    closeWhen = (
        (closeWhenSeen != nil && gPlayerChar.hasSeen(closeWhenSeen))
        || (closeWhenDescribed != nil && closeWhenDescribed.described)
        || (closeWhenAchieved != nil && closeWhenAchieved.scoreCount != 0)
        || (closeWhenKnown != nil && gPlayerChar.knowsAbout(closeWhenKnown))
        || (closeWhenRevealed != nil && gRevealed(closeWhenRevealed))
        || closeWhenTrue)

    /*
     *   Has this goal been fully displayed?  The hint system automatically
     *   sets this to true when the last item in our hint list is
     *   displayed.
     *   
     *   You can use this, for example, to automatically remove the hint
     *   from the hint menu after it's been fully displayed.  (You might
     *   want to do this with a hint for a red herring, for example.  After
     *   the player has learned that the red herring is a red herring, they
     *   probably won't need to see that particular line of hints again, so
     *   you can remove the clutter in the menu by closing the hint after
     *   it's been fully displayed.)  To do this, simply add this to the
     *   Goal object:
     *   
     *.    closeWhenTrue = (goalFullyDisplayed)
     */
    goalFullyDisplayed = nil

    /*
     *   Check our menu state and update it if necessary.  Each time our
     *   parent menu is about to display, it'll call this on its sub-items
     *   to let them update their current states.  This method can promote
     *   the state to Open or Closed if the necessary conditions for the
     *   goal have been met.
     *   
     *   Sometimes it's more convenient to set a goal's state explicitly
     *   from a scripted event; for example, if the goal is associated
     *   with a scored achievement, awarding the goal's achievement will
     *   set the goal's state to Closed.  In these cases, there's no need
     *   to use this method, since you're managing the goal's state
     *   explicitly.  The purpose of this method is to make it easy to
     *   catch goal state changes that can be reached by several different
     *   routes; in these cases, you can just write a single test for
     *   those conditions in this method rather than trying to catch every
     *   possible route to the new conditions and writing code in all of
     *   those.
     *   
     *   The default implementation looks at our openWhenSeen property.
     *   If this property is not nil, then we'll check the object
     *   referenced in this property; if our current state is
     *   Undiscovered, and the object referenced by openWhenSeen has been
     *   seen by the player character, then we'll change our state to
     *   Open.  We'll make the corresponding check for openWhenDescribed.  
     */
    updateContents()
    {
        /* 
         *   If we're currently Undiscovered, and our openWhenSeen object
         *   has been seen by the player charater, change our state to
         *   Open.  Likewise, if our gating achievement has been scored,
         *   open the goal.  
         */
        if (goalState == UndiscoveredGoal && openWhen)
        {
            /* 
             *   the player has encountered our gating object, so open
             *   this goal 
             */
            goalState = OpenGoal;
        }

        /* 
         *   if we're currently Undiscovered or Open, and our Achievement
         *   has been scored, then change our state to Closed - once the
         *   goal has been achieved, there's no need to offer hints on the
         *   topic any longer 
         */
        if (goalState is in (UndiscoveredGoal, OpenGoal) && closeWhen)
        {
            /* the goal has been achieved, so close it */
            goalState = ClosedGoal;
        }
    }

    /* display a sub-item, keeping track of when we've shown them all */
    displaySubItem(idx, lastBeforeInput, eol)
    {
        /* do the inherited work */
        inherited(idx, lastBeforeInput, eol);

        /* if we just displayed the last item, note it */
        if (idx == menuContents.length())
            goalFullyDisplayed = true;
    }

    /* we're active in our parent menu if our goal state is Open */
    isActiveInMenu = (goalState == OpenGoal)

    /* 
     *   This goal's current state.  We'll start off undiscovered.  When a
     *   goal should be open from the very start of the game, this should
     *   be overridden and set to OpenGoal. 
     */
    goalState = UndiscoveredGoal
;

/*
 *   A Hint encapsulates one hint from a topic.  In many cases, hints can
 *   be listed in a topic simply as strings, rather than using Hint
 *   objects.  Hint objects provide a little more control, though; in
 *   particular, a Hint object can specify some additional code to run
 *   when the hint is shown, so that it can apply any side effects of
 *   showing the hint (for example, when a hint is shown, it could mark
 *   another Goal object as Open, which might be desirable if the hint
 *   refers to another topic that the player might not yet have
 *   encountered).  
 */
class Hint: MenuTopicSubItem
    /* the hint text */
    hintText = ''

    /*
     *   A list of other Goal objects that this hint references.  By
     *   default, when we show this hint for the first time, we'll promote
     *   each goal in this list from Undiscovered to Open.
     *   
     *   Sometimes, it's necessary to solve one puzzle before another can
     *   be solved.  In these cases, some hints for the first puzzle
     *   (which depends on the second), especially the later, more
     *   specific hints, might need to refer to the other puzzle.  This
     *   would make the player aware of the other puzzle even if they
     *   weren't already.  In such cases, it's a good idea to make sure
     *   that we make hints for the other puzzle available immediately,
     *   since otherwise the player might be confused by the absence of
     *   hints about it.  
     */
    referencedGoals = []

    /*
     *   Get my hint text.  By default, we mark as Open any goals listed
     *   in our referencedGoals list, then return our hintText string.
     *   Individual Hint objects can override this as desired to apply any
     *   additional side effects.
     */
    getItemText()
    {
        /* scan the referenced goals list */
        foreach (local cur in referencedGoals)
        {
            /* if this goal is not yet discovered, open it */
            if (cur.goalState == UndiscoveredGoal)
                cur.goalState = OpenGoal;
        }

        /* return our hint text */
        return hintText;
    }
;

/*
 *   A hint menu.  This same class can be used for the top-level hints
 *   menu and for sub-menus within the hints menu.
 *   
 *   The typical hint menu system will be structured into a top-level hint
 *   menu that contains a set of sub-menus for the main areas of the game;
 *   each sub-menu will have a series of Goal items, each Goal providing a
 *   set of answers to a particular question.  Something like this:
 *   
 *   topHintMenu: TopHintMenu 'Hints';
 *.  + HintMenu 'General Questions';
 *.  ++ Goal 'What am I supposed to be doing?' [answer, answer, answer];
 *.  ++ Goal 'Amusing things to try' [thing, thing, thing];
 *.  + HintMenu 'First Area';
 *.  ++ Goal 'How do I get past the shark?' [answer, answer, answer];
 *.  ++ Goal 'How do I open the fish tank?' [answer, answer, answer];
 *.  + HintMenu 'Second Area';
 *.  ++ Goal 'Where is the gold key?' [answer, answer, answer];
 *.  ++ Goal 'How do I unlock the gold door?' [answer, answer, answer];
 *   
 *   Note that there's no requirement that the hint menu tree takes
 *   exactly this shape.  A very small game could dispense with the
 *   submenus and simply put all of the goals directly in the top hint
 *   menu.  A very large game with lots of goals could add more levels of
 *   sub-menus to make it easier to navigate the large number of topics.  
 */
class HintMenu: MenuItem, HintMenuObject
    /* the menu's title */
    title = ''

    /* update our contents */
    updateContents()
    {
        local vec = new Vector(16);
        
        /* 
         *   First, run through all of our sub-items, and update their
         *   contents.  We only want to show our active contents, so we
         *   need to check with each item to find out which is active. 
         */
        foreach (local cur in allContents)
            cur.updateContents();

        /* create a vector containing all of our active items */
        foreach (local cur in allContents)
        {
            /* if this item is active, add it to the active vector */
            if (cur.isActiveInMenu)
                vec.append(cur);
        }

        /* set our contents list to the list of active items */
        contents = vec;
    }

    /* we're active in a menu if we have any active contents */
    isActiveInMenu = (contents.length() != 0)

    /* add a sub-item to our contents */
    addToContents(obj)
    {
        /* 
         *   add the sub-item to our allContents list rather than our
         *   active contents 
         */
        allContents += obj;
    }

    /* initialize our contents list */
    initializeContents()
    {
        /* sort our allContents list in the object-defined sorting order */
        allContents = allContents.sort(
            SortAsc, {a, b: a.compareForTopicSort(b)});
    }

    /* 
     *   our list of all of our sub-items (some of which may not be
     *   active, in which case they'll appear in this list but not in our
     *   'contents' list, which contains only active contents) 
     */
    allContents = []
;

/*
 *   A hint menu version of the long topic menu.
 */
class HintLongTopicItem: MenuLongTopicItem, HintMenuObject
    /* 
     *   presume these are always active - they're usually used for things
     *   like hint system instructions that should always be available 
     */
    isActiveInMenu = true
;

/*
 *   Top-level hint menu.  As a convenience, an object defined of this
 *   class will automatically register itself as the top-level hint menu
 *   during pre-initialization.  
 */
class TopHintMenu: HintMenu, PreinitObject
    /* register as the top-level hint menu during pre-initialization */
    execute() { hintManager.topHintMenuObj = self; }
;

/* ------------------------------------------------------------------------ */
/*
 *   The default hint system user interface implementation.  All of the
 *   hint-related verbs operate by calling methods in the object stored in
 *   the global variable gHintSystem, which we'll by default initialize
 *   with a reference to this object.  Games can replace this with their
 *   own implementations if desired.  
 */
hintManager: PreinitObject
    /* during pre-initialization, register as the global hint manager */
    execute() { gHintManager = self; }
    
    /*
     *   Disable hints - this is invoked by the HINTS OFF action.
     *   
     *   Some users don't like on-line hint systems because they find them
     *   to be too much of a temptation.  To address this concern, we
     *   provide this HINTS OFF command.  Players who want to ensure that
     *   their will-power won't crumble later on in the face of a
     *   difficult puzzle can type HINTS OFF early on, before the going
     *   gets rough; this will disable hints for the rest of the session.
     *   It's kind of like giving your credit card to a friend before
     *   going to the mall, making the friend promise that they won't let
     *   you spend more than such and such an amount, no matter how much
     *   you beg and plead.  
     */
    disableHints()
    {
        /* 
         *   Remember that hints have been disabled.  Keep this
         *   information in the transient session object, since we want
         *   the disabled status to last for the rest of this session,
         *   even if we restore or restart later.  
         */
        sessionHintStatus.hintsDisabled = true;

        /* acknowledge it */
        mainReport(gLibMessages.hintsDisabled);
    }

    /*
     *   The top-level hint menu.  This must be provided by the game, and
     *   should be set during initialization.  If this is nil, hints won't
     *   be available.
     *   
     *   We don't provide a default top-level hint menu because we want to
     *   give the game maximum flexibility in defining this object exactly
     *   as it wants.  For convenience, an object of class TopHintMenu
     *   will automatically register itself during pre-initialization -
     *   but note that there should be only one such object in the entire
     *   game, since if there are more than one, only one will be
     *   arbitrarily chosen as the registered object.  
     */
    topHintMenuObj = nil

    /*
     *   Show hints - invoke the hint system. 
     */
    showHints()
    {
        /* if there is no top-level hint menu, no hints are available */
        if (topHintMenuObj == nil)
        {
            mainReport(gLibMessages.hintsNotPresent);
            return;
        }

        /* if hints are disabled, reject the request */
        if (sessionHintStatus.hintsDisabled)
        {
            mainReport(gLibMessages.sorryHintsDisabled);
            return;
        }

        /* bring the hint menu tree up to date */
        topHintMenuObj.updateContents();

        /* if there are no hints available, say so and give up */
        if (topHintMenuObj.contents.length() == 0)
        {
            mainReport(gLibMessages.currentlyNoHints);
            return;
        }
        
        /* if we haven't warned about hints, do so now */
        if (!showHintWarning())
            return;

        /* display the hint menu */
        topHintMenuObj.display();

        /* all done */
        mainReport(gLibMessages.hintsDone);
    }

    /*
     *   Show a warning before showing any hints.  By default, we'll show
     *   this at most once per session or once per saved game.  Returns
     *   true if we are to proceed to the hints, nil if not.  
     */
    showHintWarning()
    {
        /* 
         *   If we have previously warned in this session, or if we've
         *   warned in a previous session and the same game was later
         *   saved and restored, don't warn again.  The transient session
         *   object tells us if we've asked in this session; the normal
         *   persistent object tells us if we've asked in a previous
         *   session that we've since saved and restored. 
         */
        if (!sessionHintStatus.hintWarning && !gameHintStatus.hintWarning)
        {
            /* 
             *   we haven't asked yet in either the session or the game,
             *   so show the warning now 
             */
            gLibMessages.showHintWarning();

            /* note that we've shown the warning */
            sessionHintStatus.hintWarning = true;
            gameHintStatus.hintWarning = true;

            /* don't proceed to hints now; let them ask again */
            return nil;
        }

        /* 
         *   They've already seen the warning before.  It's possible that
         *   they've seen it in a past session with the game and not
         *   otherwise during this session, but now that we're accessing
         *   the hint system once, don't bother with another warning for
         *   the rest of this session.  
         */
        sessionHintStatus.hintWarning = true;

        /* proceed to the hints */
        return true;
    }
;

/*
 *   We keep several pieces of information about the status of the hint
 *   system.  Some of it pertains to the current session, independently of
 *   any saving/restoring/restarting, so we keep this information in a
 *   transient object.  Some pertains to the present game, so we keep it
 *   in an ordinary persistent object, so that it's saved and restored
 *   along with the game.  
 */
transient sessionHintStatus: object
    /* flag: we've warned about the hint system in this session */
    hintWarning = nil

    /* flag: we've disabled hints for this session */
    hintsDisabled = nil
;

gameHintStatus: object
    /* flag: we've warned about the hint system in this session */
    hintWarning = nil
;

