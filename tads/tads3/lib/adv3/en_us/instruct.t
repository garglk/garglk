#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: Instructions for new players
 *   
 *   This module defines the INSTRUCTIONS command, which provides the
 *   player with an overview of how to play IF games in general.  These
 *   instructions are especially designed as an introduction to IF for
 *   inexperienced players.  The instructions given here are meant to be
 *   general enough to apply to most games that follow the common IF
 *   conventions. 
 *   
 *   This module defines the English version of the instructions.
 *   
 *   In most cases, each author should customize these general-purpose
 *   instructions at least a little for the specific game.  We provide a
 *   few hooks for some specific parameter-driven customizations that don't
 *   require modifying the original text in this file.  Authors should also
 *   feel free to make more extensive customizations as needed to address
 *   areas where the game diverges from the common conventions described
 *   here.
 *   
 *   One of the most important things you should do to customize these
 *   instructions for your game is to add a list of any special verbs or
 *   command phrasings that your game uses.  Of course, you might think
 *   you'll be spoiling part of the challenge for the player if you do
 *   this; you might worry that you'll give away a puzzle if you don't keep
 *   a certain verb secret.  Be warned, though, that many players - maybe
 *   even most - don't think "guess the verb" puzzles are good challenges;
 *   a lot of players feel that puzzles that hinge on finding the right
 *   verb or phrasing are simply bad design that make a game less
 *   enjoyable.  You should think carefully about exactly why you don't
 *   want to disclose a particular verb in the instructions.  If you want
 *   to withhold a verb because the entire puzzle is to figure out what
 *   command to use, then you have created a classic guess-the-verb puzzle,
 *   and most everyone in the IF community will feel this is simply a bad
 *   puzzle that you should omit from your game.  If you want to withhold a
 *   verb because it's too suggestive of a particular solution, then you
 *   should at least make sure that a more common verb - one that you are
 *   willing to disclose in the instructions, and one that will make as
 *   much sense to players as your secret verb - can achieve the same
 *   result.  You don't have to disclose every *accepted* verb or phrasing
 *   - as long as you disclose every *required* verb *and* phrasing, you
 *   will have a defense against accusations of using guess-the-verb
 *   puzzles.
 *   
 *   You might also want to mention the "cruelty" level of the game, so
 *   that players will know how frequently they should save the game.  It's
 *   helpful to point out whether or not it's possible for the player
 *   character to be killed; whether it's possible to get into situations
 *   where the game becomes "unwinnable"; and, if the game can become
 *   unwinnable, whether or not this will become immediately clear.  The
 *   kindest games never kill the PC and are always winnable, no matter
 *   what actions the player takes; it's never necessary to save these
 *   games except to suspend a session for later resumption.  The cruelest
 *   games kill the PC without warning (although if they offer an UNDO
 *   command from a "death" prompt, then even this doesn't constitute true
 *   cruelty), and can become unwinnable in ways that aren't readily and
 *   immediately apparent to the player, which means that the player could
 *   proceed for quite some time (and thus invest substantial effort) after
 *   the game is already effectively lost.  Note that unwinnable situations
 *   can often be very subtle, and might not even be intended by the
 *   author; for example, if the player needs a candle to perform an
 *   exorcism at some point, but the candle can also be used for
 *   illumination in dark areas, the player could make the game unwinnable
 *   simply by using up the candle early on while exploring some dark
 *   tunnels, and might not discover the problem until much further into
 *   the game.  
 */

#include "adv3.h"
#include "en_us.h"

/*
 *   The INSTRUCTIONS command.  Make this a "system" action, because it's
 *   a meta-action outside of the story.  System actions don't consume any
 *   game time.  
 */
DefineSystemAction(Instructions)
    /*
     *   This property tells us how complete the verb list is.  By default,
     *   we'll assume that the instructions fail to disclose every required
     *   verb in the game, because the generic set we use here doesn't even
     *   try to anticipate the special verbs that most games include.  If
     *   you provide your own list of game-specific verbs, and your custom
     *   list (taken together with the generic list) discloses every verb
     *   required to complete the game, you should set this property to
     *   true; if you set this to true, the instructions will assure the
     *   player that they will not need to think of any verbs besides the
     *   ones listed in the instructions.  Authors are strongly encouraged
     *   to disclose a list of verbs that is sufficient by itself to
     *   complete the game, and to set this property to true once they've
     *   done so.  
     */
    allRequiredVerbsDisclosed = nil

    /* 
     *   A list of custom verbs.  Each game should set this to a list of
     *   single-quoted strings; each string gives an example of a verb to
     *   display in the list of sample verbs.  Something like this:
     *   
     *   customVerbs = ['brush my teeth', 'pick the lock'] 
     */
    customVerbs = []

    /* 
     *   Verbs relating specifically to character interaction.  This is in
     *   the same format as customVerbs, and has essentially the same
     *   purpose; however, we call these out separately to allow each game
     *   not only to supplement the default list we provide but to replace
     *   our default list.  This is desirable for conversation-related
     *   commands in particular because some games will not use the
     *   ASK/TELL conversation system at all and will thus want to remove
     *   any mention of the standard set of verbs.  
     */
    conversationVerbs =
    [
        'ASK WIZARD ABOUT WAND',
        'ASK WIZARD FOR POTION',
        'TELL WIZARD ABOUT DUSTY TOME',
        'SHOW SCROLL TO WIZARD',
        'GIVE WAND TO WIZARD',
        'YES (or NO)'
    ]

    /* conversation verb abbreviations */
    conversationAbbr = "\n\tASK ABOUT (topic) can be abbreviated
                        to A (topic)
                        \n\tTELL ABOUT (topic) can be entered as T (topic)"

    /*
     *   Truncation length. If the game's parser allows words to be
     *   abbreviated to some minimum number of letters, this should
     *   indicate the minimum length.  The English parser uses a truncation
     *   length of 6 letters by default.
     *   
     *   Set this to nil if the game doesn't allow truncation at all.  
     */
    truncationLength = 6

    /*
     *   This property should be set on a game-by-game basis to indicate
     *   the "cruelty level" of the game, which is a rough estimation of
     *   how likely it is that the player will encounter an unwinnable
     *   position in the game.
     *   
     *   Level 0 is "kind," which means that the player character can
     *   never be killed, and it's impossible to make the game unwinnable.
     *   When this setting is used, the instructions will reassure the
     *   player that saving is necessary only to suspend the session.
     *   
     *   Level 1 is "standard," which means that the player character can
     *   be killed, and/or that unwinnable positions are possible, but
     *   that there are no especially bad unwinnable situations.  When
     *   this setting is selected, we'll warn the player that they should
     *   save every so often.
     *   
     *   (An "especially bad" situation is one in which the game becomes
     *   unwinnable at some point, but this won't become apparent to the
     *   player until much later.  For example, suppose the first scene
     *   takes place in a location that can never be reached again after
     *   the first scene, and suppose that there's some object you can
     *   obtain in this scene.  This object will be required in the very
     *   last scene to win the game; if you don't have the object, you
     *   can't win.  This is an "especially bad" unwinnable situation: if
     *   you leave the first scene without getting the necessary object,
     *   the game is unwinnable from that point forward.  In order to win,
     *   you have to go back and play almost the whole game over again.
     *   Saved positions are almost useless in a case like this, since
     *   most of the saved positions will be after the fatal mistake; no
     *   matter how often you saved, you'll still have to go back and do
     *   everything over again from near the beginning.)
     *   
     *   Level 2 is "cruel," which means that the game can become
     *   unwinnable in especially bad ways, as described above.  If this
     *   level is selected, we'll warn the player more sternly to save
     *   frequently.
     *   
     *   We set this to 1 ("standard") by default, because even games that
     *   aren't intentionally designed to be cruel often have subtle
     *   situations where the game becomes unwinnable, because of things
     *   like the irreversible loss of an object, or an unrepeatable event
     *   sequence; it almost always takes extra design work to ensure that
     *   a game is always winnable.  
     */
    crueltyLevel = 1

    /*
     *   Does this game have any real-time features?  If so, set this to
     *   true.  By default, we'll explain that game time passes only in
     *   response to command input. 
     */
    isRealTime = nil

    /*
     *   Conversation system description.  Several different conversation
     *   systems have come into relatively widespread use, so there isn't
     *   any single convention that's generic enough that we can assume it
     *   holds for all games.  In deference to this variability, we
     *   provide this hook to make it easy to replace the instructions
     *   pertaining to the conversation system.  If the game uses the
     *   standard ASK/TELL system, it can leave this list unchanged; if
     *   the game uses a different system, it can replace this with its
     *   own instructions.
     *   
     *   We'll include information on the TALK TO command if there are any
     *   in-conversation state objects in the game; if not, we'll assume
     *   there's no need for this command.
     *   
     *   We'll mention the TOPICS command if there are any SuggestedTopic
     *   instances in the game; if not, then the game will never have
     *   anything to suggest, so the TOPICS command isn't needed.
     *   
     *   We'll include information on special topics if there are any
     *   SpecialTopic objects defined.  
     */
    conversationInstructions =
        "You can talk to other characters by asking or
        telling them about things in the story.  For example, you might
        ASK WIZARD ABOUT WAND or TELL GUARD ABOUT ALARM.  You should
        always use the ASK ABOUT or TELL ABOUT phrasing; the story
        won&rsquo;t be able to understand other formats, so you don&rsquo;t
        have to worry about thinking up complicated questions like <q>ask
        guard how to open the window.</q>
        In most cases, you&rsquo;ll get the best results by asking
        about specific objects or other characters you&rsquo;ve encountered
        in the story, rather than about abstract topics such as
        MEANING OF LIFE; however, if something in the story leads you
        to believe you <i>should</i> ask about some particular abstract
        topic, it can&rsquo;t hurt to try.

        \bIf you&rsquo;re asking or telling the same person about several
        topics in succession, you can save some typing by abbreviating
        ASK ABOUT to A, and TELL ABOUT to T.  For example, once you&rsquo;re
        talking to the wizard, you can abbreviate ASK WIZARD ABOUT AMULET
        to simply A AMULET.  This addresses the question to the same
        character as in the last ASK or TELL.

        <<firstObj(InConversationState, ObjInstances) != nil ?
          "\bTo greet another character, type TALK TO (Person).  This
          tries to get the other character&rsquo;s attention and start a
          conversation.  TALK TO is always optional, since you can start
          in with ASK or TELL directly if you prefer." : "">>

        <<firstObj(SpecialTopic, ObjInstances) != nil ?
          "\bThe story might occasionally suggest some special conversation
          commands, like this:

          \b\t(You could apologize, or explain about the aliens.)

          \bIf you like, you can use one of the suggestions just by
          typing in the special phrasing shown.  You can usually
          abbreviate these to the first few words when they&rsquo;re long.

          \b\t&gt;APOLOGIZE
          \n\t&gt;EXPLAIN ABOUT ALIENS

          \bSpecial suggestions like this only work right at the moment
          they&rsquo;re offered, so you don&rsquo;t have to worry about
          memorizing them, or trying them at other random times in the story.
          They&rsquo;re not new commands for you to learn; they&rsquo;re just
          extra options you have at specific times, and the story will always
          let you know when they&rsquo;re available.  When the story offers
          suggestions like this, they don&rsquo;t limit what you can do; you
          can still type any ordinary command instead of one of the
          suggestions." : "">>

        <<firstObj(SuggestedTopic, ObjInstances) != nil ?
          "\bIf you&rsquo;re not sure what to discuss, you can type TOPICS any
          time you&rsquo;re talking to someone.  This will show you a list of
          things that your character might be interested in discussing
          with the other person.  The TOPICS command usually won&rsquo;t list
          everything that you can discuss, so feel free to explore other
          topics even if they&rsquo;re not listed." : "">>

        \bYou can also interact with other characters using physical
        objects.  For example, you might be able to give something to
        another character, as in GIVE MONEY TO CLERK, or show an object
        to someone, as in SHOW IDOL TO PROFESSOR.  You might also be
        able to fight other characters, as in ATTACK TROLL WITH
        SWORD or THROW AXE AT DWARF.

        \bIn some cases, you can tell a character to do
        something for you.  You do this by typing the character&rsquo;s name,
        then a comma, then the command you want the character to perform,
        using the same wording you&rsquo;d use for a command to your own
        character.  For example:

        \b\t&gt;ROBOT, GO NORTH

        \bKeep in mind, though, that there&rsquo;s no guarantee that other
        characters will always obey your orders.  Most characters have
        minds of their own and won&rsquo;t automatically do whatever you
        ask. "

    /* execute the command */
    execSystemAction()
    {
        local origElapsedTime;

        /* 
         *   note the elapsed game time on the real-time clock before we
         *   start, so that we can reset the game time when we're done; we
         *   don't want the instructions display to consume any real game
         *   time 
         */
        origElapsedTime = realTimeManager.getElapsedTime();

        /* show the instructions */
        showInstructions();

        /* reset the real-time game clock */
        realTimeManager.setElapsedTime(origElapsedTime);
    }

#ifdef INSTRUCTIONS_MENU
    /*
     *   Show the instructions, using a menu-based table of contents.
     */
    showInstructions()
    {
        /* run the instructions menu */
        topInstructionsMenu.display();

        /* show an acknowledgment */
        "Done. ";
    }
    
#else /* INSTRUCTIONS_MENU */

    /*
     *   Show the instructions as a standard text display.  Give the user
     *   the option of turning on a SCRIPT file to capture the text.  
     */
    showInstructions()
    {
        local startedScript;

        /* presume we won't start a new script file */
        startedScript = nil;
        
        /* show the introductory message */
        "The story is about to show a full set of instructions,
        designed especially for people who aren&rsquo;t already familiar
        with interactive fiction.  The instructions are lengthy";

        /*
         *   Check to see if we're already scripting.  If we aren't, offer
         *   to save the instructions to a file. 
         */
        if (scriptStatus.scriptFile == nil)
        {
            local str;
            
            /* 
             *   they're not already logging; ask if they'd like to start
             *   doing so 
             */
            ", so you might want to capture them to a file (so that
            you can print them out, for example).  Would you like to
            proceed?
            \n(<<aHref('yes', 'Y')>> is affirmative, or type
            <<aHref('script', 'SCRIPT')>> to capture to a file) &gt; ";

            /* ask for input */
            str = inputManager.getInputLine(nil, nil);

            /* if they want to capture them to a file, set up scripting */
            if (rexMatch('<nocase><space>*s(c(r(i(pt?)?)?)?)?<space>*', str)
                == str.length())
            {
                /* try setting up a scripting file */
                ScriptAction.setUpScripting(nil);

                /* if that failed, don't proceed */
                if (scriptStatus.scriptFile == nil)
                    return;
                
                /* note that we've started a script file */
                startedScript = true;
            }
            else if (rexMatch('<nocase><space>*y.*', str) != str.length())
            {
                "Canceled. ";
                return;
            }
        }
        else
        {
            /* 
             *   they're already logging; just confirm that they want to
             *   see the instructions 
             */
            "; would you like to proceed?
            \n(Y is affirmative) &gt; ";

            /* stop if they don't want to proceed */
            if (!yesOrNo())
            {
                "Canceled. ";
                return;
            }
        }

        /* make sure we have something for the next "\b" to skip from */
        "\ ";

        /* show each chapter in turn */
        showCommandsChapter();
        showAbbrevChapter();
        showTravelChapter();
        showObjectsChapter();
        showConversationChapter();
        showTimeChapter();
        showSaveRestoreChapter();
        showSpecialCmdChapter();
        showUnknownWordsChapter();
        showAmbiguousCmdChapter();
        showAdvancedCmdChapter();
        showTipsChapter();

        /* if we started a script file, close it */
        if (startedScript)
            ScriptOffAction.turnOffScripting(nil);
    }

#endif /* INSTRUCTIONS_MENU */

    /* Entering Commands chapter */
    showCommandsChapter()
    {
        "\b<b>Entering Commands</b>\b
        You&rsquo;ve probably already noticed that you interact with the story
        by typing a command whenever you see the <q>prompt,</q> which
        usually looks like this:
        \b";

        gLibMessages.mainCommandPrompt(rmcCommand);

        "\bKnowing this much, you&rsquo;re probably thinking one of two things:
        <q>Great, I can type absolutely anything I want, in plain English,
        and the story will do my bidding,</q> or <q>Great, now I have to
        figure out yet another heinously complex command language for
        a computer program; I think I&rsquo;ll go play Minesweeper.</q>
        Well, neither extreme is quite true.

        \bIn actual play, you&rsquo;ll only need a fairly small set of
        commands, and the commands are mostly in ordinary English, so
        there&rsquo;s not very much you&rsquo;ll have to learn or remember.
        Even though that command prompt can look intimidating, don&rsquo;t
        let it scare you off &mdash; there are just a few simple things you
        have to know.

        \bFirst, you&rsquo;ll almost never have to refer to anything that
        isn&rsquo;t directly mentioned in the story; this is a story, after
        all, not a guessing game where you have to think of everything that
        goes together with some random object.  For example, if
        you&rsquo;re wearing a jacket, you might assume that the jacket has
        pockets, or buttons, or a zipper &mdash; but if the story never
        mentions those things, you shouldn&rsquo;t have to worry about them.

        \bSecond, you won&rsquo;t have to think of every conceivable action
        you could perform.  The point of the game isn&rsquo;t to make you
        guess at verbs.  Instead, you&rsquo;ll only have to use a relatively
        small number of simple, ordinary actions.  To give you an idea
        of what we mean, here are some of the commands you can use:";
        
        "\b
        \n\t LOOK AROUND
        \n\t INVENTORY
        \n\t GO NORTH (or EAST, SOUTHWEST, and so on, or UP, DOWN, IN, OUT)
        \n\t WAIT
        \n\t TAKE THE BOX
        \n\t DROP THE DISK
        \n\t LOOK AT THE DISK
        \n\t READ THE BOOK
        \n\t OPEN BOX
        \n\t CLOSE BOX
        \n\t LOOK IN THE BOX
        \n\t LOOK THROUGH WINDOW
        \n\t PUT FLOPPY DISK INTO BOX
        \n\t PUT BOX ON TABLE
        \n\t WEAR THE CONICAL HAT
        \n\t TAKE OFF HAT
        \n\t TURN ON LANTERN
        \n\t LIGHT MATCH
        \n\t LIGHT CANDLE WITH MATCH
        \n\t PUSH BUTTON
        \n\t PULL LEVER
        \n\t TURN KNOB
        \n\t TURN DIAL TO 11
        \n\t EAT COOKIE
        \n\t DRINK MILK
        \n\t THROW PIE AT CLOWN
        \n\t ATTACK TROLL WITH SWORD
        \n\t UNLOCK DOOR WITH KEY
        \n\t LOCK DOOR WITH KEY
        \n\t CLIMB THE LADDER
        \n\t GET IN THE CAR
        \n\t SIT ON THE CHAIR
        \n\t STAND ON THE TABLE
        \n\t STAND IN FLOWER BED
        \n\t LIE ON THE BED
        \n\t TYPE HELLO ON COMPUTER
        \n\t LOOK UP BOB IN PHONE BOOK";

        /* show the conversation-related verbs */
        foreach (local cur in conversationVerbs)
            "\n\t <<cur>>";

        /* show the custom verbs */
        foreach (local cur in customVerbs)
            "\n\t <<cur>>";

        /* 
         *   if the list is exhaustive, say so; otherwise, mention that
         *   there might be some other verbs to find 
         */
        if (allRequiredVerbsDisclosed)
            "\bThat&rsquo;s it &mdash; every verb and every command phrasing
            you need to complete the story is shown above.
            If you ever get stuck and feel like the story is expecting
            you to think of something completely out of the blue, remember
            this: whatever it is you have to do, you can do it with one
            or more of the commands shown above. 
            Don&rsquo;t ever worry that you have to start trying lots of
            random commands to hit upon the magic phrasing, because
            everything you need is shown plainly above. ";
        else
            "\bMost of the verbs you&rsquo;ll need to complete the story are
            shown above; there might be a few additional commands you&rsquo;ll
            need from time to time as well, but they&rsquo;ll follow the same
            simple format as the commands above.";

        "\bA few of these commands deserve some more explanation.
        LOOK AROUND (which you abbreviate to LOOK, or even just L)
        shows the description of the current location.  You can use
        this if you want to refresh your memory of your character&rsquo;s
        surroundings.  INVENTORY (or just I) shows a list of everything
        your character is carrying. WAIT (or Z) just lets a little
        time pass in the story.";
    }

    /* Abbreviations chapter */
    showAbbrevChapter()
    {
        "\b<b>Abbreviations</b>
        \bYou&rsquo;ll probably use a few commands quite a lot, so to save
        typing, you can abbreviate some of the most frequently-used
        commands:

        \b
        \n\t LOOK AROUND can be shortened to LOOK, or merely L
        \n\t INVENTORY can be simply I
        \n\t GO NORTH can be written NORTH, or even just N (likewise E, W, S,
            NE, SE, NW, SW, U for UP and D for DOWN)
        \n\t LOOK AT THE DISK can be entered as EXAMINE DISK or simply X DISK
        \n\t WAIT can be shortened to Z
        <<conversationAbbr>>

        \b<b>A Few More Command Details</b>
        \bWhen you&rsquo;re entering commands, you can use capital or small
        letters in any mixture.  You can use words such as THE and A
        when they&rsquo;re appropriate, but you can omit them if you
        prefer. ";

        if (truncationLength != nil)
        {
            "You can abbreviate any word to its first <<
            spellInt(truncationLength)>> letters, but if you choose not
            to abbreviate, the story will pay attention to
            everything you actually type; this means, for example, that you
            can abbreviate SUPERCALIFRAGILISTICEXPIALIDOCIOUS to <<
            'SUPERCALIFRAGILISTICEXPIALIDOCIOUS'.substr(1, truncationLength)
            >> or <<
            'SUPERCALIFRAGILISTICEXPIALIDOCIOUS'.substr(1, truncationLength+2)
            >>, but not to <<
            'SUPERCALIFRAGILISTICEXPIALIDOCIOUS'.substr(1, truncationLength)
            >>SDF. ";
        }
    }

    /* Travel chapter */
    showTravelChapter()
    {
        "\b<b>Travel</b>
        \bAt any given time in the story, your character is in a
        <q>location.</q>  The story describes the location when your
        character first enters, and again any time you type LOOK.
        Each location usually has a short name that&rsquo;s displayed just
        before its full description; the name is useful when drawing a map,
        and the short name can help jog your memory as you&rsquo;re finding
        your way around.

        \bEach location is a separate room, or a large outdoor area,
        or the like.  (Sometimes a single physical room is so large that
        it comprises several locations in the game, but that&rsquo;s fairly
        rare.)  For the most part, going to a location is as specific
        as you have to get about travel; once your character is in a
        location, the character can usually see and reach everything
        within the location, so you don&rsquo;t have to worry about where
        exactly your character is standing within the room.  Once in
        a while, you might find that something is out of reach, perhaps
        because it&rsquo;s on a high shelf or on the other side of a moat; in
        these cases, it&rsquo;s sometimes useful to be more specific about
        your character&rsquo;s location, such as by standing on something
        (STAND ON TABLE, for example).

        \bTraveling from one location to another is usually done using
        a direction command: GO NORTH, GO NORTHEAST, GO UP, and so on.
        (You can abbreviate the cardinal and vertical directions to one
        letter each &mdash; N, S, E, W, U, D &mdash; and the diagonal
        directions to two: NE, NW, SE, SW.)  The story should always
        tell you the directions that you can go when it describes a
        location, so you should never have to try all the unmentioned
        directions to see if they go anywhere.

        \bIn most cases, backtracking (by reversing the direction you
        took from one location to another) will take you back to where you
        started, although some passages might have turns.

        \bMost of the time, when the story describes a door or passageway,
        you won&rsquo;t need to open the door to go through the passage, as
        the story will do this for you.  Only when the story specifically
        states that a door is blocking your way will you usually have to
        deal with the door explicitly.";
    }

    /* Objects chapter */
    showObjectsChapter()
    {
        "\b<b>Manipulating Objects</b>
        \bYou might find objects in the story that your character can
        carry or otherwise manipulate.  If you want to carry something,
        type TAKE and the object&rsquo;s name: TAKE BOOK.  If you want to
        drop something your character is carrying, DROP it.

        \bYou usually won&rsquo;t have to be very specific about exactly
        how your character is supposed to carry or hold something, so you
        shouldn&rsquo;t have to worry about which hand is holding which item
        or anything like that.  It might sometimes be useful to put one
        object inside or on top of another, though; for example, PUT BOOK
        IN SHOPPING BAG or PUT VASE ON TABLE.  If your character&rsquo;s
        hands get full, it might help to put some items being carried
        into a container, much as in real life you can carry more stuff
        if it&rsquo;s in a bag or a box.

        \bYou can often get a lot of extra information (and sometimes 
        vital clues) by examining objects, which you can do with the LOOK
        AT command (or, equivalently, EXAMINE, which you can abbreviate
        to simply X, as in X PAINTING).  Most experienced players get
        in the habit of examining everything in a new location right
        away. ";
    }

    /* show the Conversation chapter */
    showConversationChapter()
    {
        "\b<b>Interacting with Other Characters</b>
        \bYour character may encounter other people or creatures in the
        story.  You can sometimes interact with these characters.\b";

        /* show the customizable conversation instructions */
        conversationInstructions;
    }

    /* Time chapter */
    showTimeChapter()
    {
        "\b<b>Time</b>";

        if (isRealTime)
        {
            "\bTime passes in the story in response to the commands
            you type.  In addition, some parts of the story
            might unfold in <q>real time,</q> which means that things
            can happen even while you&rsquo;re thinking about your next move.

            \bIf you want to stop the clock while you&rsquo;re away from your
            computer for a moment (or you just need some time to think),
            you can type PAUSE.";
        }
        else
        {
            "\bTime passes in the story only in response to commands
            you type.  This means that nothing happens while the story
            is waiting for you to type something.  Each command takes
            about the same amount of time in the story.  If you
            specifically want to let some extra time pass within the
            story, because you think something is about to happen,
            you can type WAIT (or just Z). ";
        }
    }

    /* Saving, Restoring, and Undo chapter */
    showSaveRestoreChapter()
    {
        "\b<b>Saving and Restoring</b>
        \bYou can save a snapshot of your current position in the story
        at any time, so that you can later restore the story to the
        same position.  The snapshot will be saved to a file on your
        computer&rsquo;s disk, and you can save as many different snapshots
        as you&rsquo;d like (to the extent you have space on your disk,
        anyway).\b";

        switch (crueltyLevel)
        {
        case 0:
            "In this story, your character will never be killed, and
            you&rsquo;ll never find yourself in a situation where
            it&rsquo;s impossible to complete the story.  Whatever happens
            to your character, you&rsquo;ll always be able to find a way
            to complete the story. So, unlike in many text games, you
            don&rsquo;t have to worry about saving positions to protect
            yourself against getting stuck in impossible situations.  Of
            course, you can still save as often as you&rsquo;d like, to
            suspend your session and return to it later, or to save
            positions that you think you might want to revisit.";
            break;

        case 1:
        case 2:
            "It might be possible for your character to be killed in
            the story, or for you to find your character in an impossible
            situation where you won&rsquo;t be able to complete the story.
            So, you should be sure to save your position
            <<crueltyLevel == 1 ? 'from time to time' : 'frequently'>>
            so that you won&rsquo;t have to go back too far if this should
            happen. ";

            if (crueltyLevel == 2)
                "(You should make a point of keeping all of your old saved
                positions, too, because you might not always realize
                immediately when a situation has become impossible.
                You might find at times that you need to go back further
                than just the last position that you <i>thought</i>
                was safe.)";

            break;
        }

        "\bTo save your position, type SAVE at the command prompt.
        The story will ask you for the name of a disk file to use to store
        snapshot.  You&rsquo;ll have to specify a filename suitable for
        your computer system, and the disk will need enough free space
        to store the file; you&rsquo;ll be told if there&rsquo;s any problem
        saving the file.  You should use a filename that doesn&rsquo;t already
        exist on your machine, because the new file will overwrite any
        existing file with the same name.

        \bYou can restore a previously saved position by typing RESTORE
        at any prompt.  The story will ask you for the name of the file
        to restore.  After the computer reads the file, everything in
        the story will be exactly as it was when you saved that file.";

        "\b<b>Undo</b>
        \bEven if you haven&rsquo;t saved your position recently, you can
        usually take back your last few commands with the UNDO command.
        Each time you type UNDO, the story reverses the effect of one command,
        restoring the story as it was before you typed that command.  UNDO
        is limited to taking back the last few commands, so this isn&rsquo;t
        a substitute for SAVE/RESTORE, but it&rsquo;s very handy if you
        find your character unexpectedly in a dangerous situation, or you
        make a mistake you want to take back.";
    }

    /* Other Special Commands chapter */
    showSpecialCmdChapter()
    {
        "\b<b>Some Other Special Commands</b>
        \bThe story understands a few other special commands that
        you might find useful.

        \bAGAIN (or just G): Repeats the last command.  (If your last input
        line had several commands, only the last single command on the line
        is repeated.)
        \bINVENTORY (or just I): Shows what your character is carrying.
        \bLOOK (or just L): Shows the full description of your
        character&rsquo;s current location.";

        /* if the exit lister module is active, mention the EXITS command */
        if (gExitLister != nil)
            "\bEXITS: Shows the list of obvious exits from the current
            location.
            \bEXITS ON/OFF/STATUS/LOOK: Controls the way the game
            displays exit lists.  EXITS ON puts a list of
            exits in the status line and also lists exits in each room
            description.  EXITS OFF turns off both of these listings.
            EXITS STATUS turns on just the status line exit list, and
            EXITS ROOM turns on only the room description lists.";
        
        "\bOOPS: Corrects a single misspelled word in a command, without
        retyping the entire command.  You can only use OOPS immediately
        after the story tells you it didn&rsquo;t recognize a word in your
        previous command.  Type OOPS followed by the corrected word.
        \bQUIT (or just Q): Terminates the story.
        \bRESTART: Starts the story over from the beginning.
        \bRESTORE: Restores a position previously saved with SAVE.
        \bSAVE: Saves the current position in a disk file.
        \bSCRIPT: Starts making a transcript of your story session,
        saving all of the displayed text to a disk file, so that you
        can peruse it later or print it out.
        \bSCRIPT OFF: Ends a transcript that you started with SCRIPT. 
        \bUNDO: Takes back the last command.
        \bSAVE DEFAULTS: Saves your current settings for things like
        NOTIFY, EXITS, and FOOTNOTES as defaults.  This means that your
        settings will be restored automatically the next time you start
        a new game, or RESTART this one.
        \bRESTORE DEFAULTS: Explicitly restores the option settings
        you previously saved with SAVE DEFAULTS. ";
    }
    
    /* Unknown Words chapter */
    showUnknownWordsChapter()
    {
        "\b<b>Unknown Words</b>
        \bThe story doesn&rsquo;t pretend to know every word in the English
        language.  The story might even occasionally use words in its
        own descriptions that it doesn&rsquo;t understand in commands.  If
        you type a command with a word the story doesn&rsquo;t know, the
        story will tell you which word it didn&rsquo;t recognize.  If the
        story doesn&rsquo;t know a word for something it described, and
        it doesn&rsquo;t know any synonyms for that thing, you can probably
        assume that the object is just there as a detail of the setting,
        and isn&rsquo;t important to the story. ";
    }

    /* Ambiguous Commands chapter */
    showAmbiguousCmdChapter()
    {
        "\b<b>Ambiguous Commands</b>
        \bIf you type a command that leaves out some important information,
        the story will try its best to figure out what you mean.  Whenever
        it&rsquo;s reasonably obvious from context what you mean, the story
        will make an assumption about the missing information and proceed
        with the command.  The story will point out what it&rsquo;s assuming
        in these cases, to avoid any confusion from a mismatch between the
        story&rsquo;s assumptions and yours.  For example:

        \b
        \n\t &gt;TIE THE ROPE
        \n\t (to the hook)
        \n\t The rope is now tied to the hook.  The end of the
        \n\t rope nearly reaches the floor of the pit below.
        
        \bIf the command is ambiguous enough that the story can&rsquo;t
        safely make an assumption, you&rsquo;ll be asked for more
        information.  You can answer these questions by typing the
        missing information.

        \b
        \n\t &gt;UNLOCK THE DOOR
        \n\t What do you want to unlock it with?
        \b
        \n\t &gt;KEY
        \n\t Which key do you mean, the gold key, or the silver key?
        \b
        \n\t &gt;GOLD
        \n\t Unlocked.

        \bIf the story asks you one of these questions, and you decide
        that you don&rsquo;t want to proceed with the original command after
        all, you can just type in a brand new command instead of answering
        the question.";
    }

    /* Advance Command Formats chapter */
    showAdvancedCmdChapter()
    {
        "\b<b>Advanced Command Formats</b>
        \bOnce you get comfortable with entering commands, you might
        be interested to know about some more complex command formats
        that the story will accept.  These advanced commands are all
        completely optional, because you can always do the same things
        with the simpler formats we&rsquo;ve talked about so far, but
        experienced players often like the advanced formats because
        they can save you a little typing.

        \b<b>Using Multiple Objects at Once</b>
        \bIn most commands, you can operate on multiple objects in
        a single command.  Use the word AND, or a comma, to separate
        one object from another:

        \b
        \n\t TAKE THE BOX, THE FLOPPY DISK, AND THE ROPE
        \n\t PUT DISK AND ROPE IN BOX
        \n\t DROP BOX AND ROPE
        
        \bYou can use the words ALL and EVERYTHING to refer to everything
        applicable to your command, and you can use EXCEPT or BUT
        (right after the word ALL) to exclude specific objects:

        \b
        \n\t TAKE ALL
        \n\t PUT ALL BUT DISK AND ROPE INTO BOX
        \n\t TAKE EVERYTHING OUT OF THE BOX
        \n\t TAKE ALL OFF SHELF

        \bALL refers to everything that makes sense for your command,
        excluding things inside other objects matching the ALL.  For example,
        if you&rsquo;re carrying a box and a rope, and the box contains
        a floppy disk, typing DROP ALL will drop the box and the rope,
        and the floppy will remain in the box.

        \b<b><q>It</q> and <q>Them</q></b>
        \bYou can use IT and THEM to refer to the last object or objects
        that you used in a command:

        \b
        \n\t TAKE THE BOX
        \n\t OPEN IT
        \n\t TAKE THE DISK AND THE ROPE
        \n\t PUT THEM IN THE BOX

        \b<b>Multiple Commands At Once</b>
        \bYou can put multiple commands on a single input line by
        separating the commands with periods or the word THEN, or
        with a comma or AND.  For example:

        \b
        \n\t TAKE THE DISK AND PUT IT IN THE BOX
        \n\t TAKE BOX. OPEN IT.
        \n\t UNLOCK THE DOOR WITH THE KEY. OPEN IT, THEN GO NORTH.

        \b If the story doesn&rsquo;t understand one of the commands, it will
        tell you what it couldn&rsquo;t understand, and ignore everything after
        that on the line.";
    }

    /* General Tips chapter */
    showTipsChapter()
    {
        "\b<b>A Few Tips</b>
        \bNow that you know the technical details of entering commands,
        you might be interested in some strategy pointers.  Here are
        a few techniques that experienced interactive fiction players use
        when approaching a story.

        \bEXAMINE everything, especially when you enter a new location
        for the first time.  Looking at objects will often reveal details
        that aren&rsquo;t mentioned in the broader description of the location.
        If examining an object mentions detailed parts of the object,
        examine them as well.

        \bMake a map, if the story has more than a few locations.  When
        you encounter a new location for the first time, note all of its
        exits; this will make it easy to see at a glance if there are any
        exits you haven&rsquo;t explored yet.  If you get stuck, you can scan
        your map for any unexplored areas, where you might find what
        you&rsquo;re looking for.

        \bIf the story doesn&rsquo;t recognize a word or any of its synonyms,
        the object you&rsquo;re trying to manipulate probably isn&rsquo;t
        important to the story.  If you try manipulating something, and the
        story responds with something like <q>that isn&rsquo;t important,</q>
        you can probably just ignore the object; it&rsquo;s most likely just
        there as scenery, to make the setting more detailed.

        \bIf you&rsquo;re trying to accomplish something, and nothing you do
        seems to work, pay attention to what&rsquo;s going wrong.  If
        everything you try is met with utter dismissal (<q>nothing
        happens</q> or <q>that&rsquo;s not something you can open</q>), you
        might simply be on the wrong track; step back and think about other
        ways of approaching the problem.  If the response is something more
        specific, it might be a clue. <q>The guard says <q>you can&rsquo;t
        open that here!</q>\ and snatches the box from your hands</q> &mdash;
        this might indicate that you have to get the guard to leave, or
        that you should take the box somewhere else before you open it,
        for example.

        \bIf you get completely stuck, you might try setting aside the
        current puzzle and working on a different problem for a while.
        You might even want to save your position and take a break from
        the game.  The solution to the problem that&rsquo;s been
        thwarting your progress might come to you in a flash of insight
        when you least expect it, and those moments of insight can be
        incredibly rewarding.  Some stories are best appreciated when
        played over a period of days, weeks, or even months; if you&rsquo;re
        enjoying the story, why rush through it?

        \bFinally, if all else fails, you can try asking for help.  A
        great place to go for hints is the Usenet newsgroup
        <a href='news:rec.games.int-fiction'>rec.games.int-fiction</a>. ";

        "\n";
    }

    /* INSTRUCTIONS doesn't affect UNDO or AGAIN */
    isRepeatable = nil
    includeInUndo = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   define the INSTRUCTIONS command's grammar 
 */
VerbRule(instructions) 'instructions' : InstructionsAction
;


/* ------------------------------------------------------------------------ */
/*
 *   The instructions, rendered in menu form.  The menu format helps break
 *   up the instructions by dividing the instructions into chapters, and
 *   displaying a menu that lists the chapters.  This way, players can
 *   easily go directly to the chapters they're most interested in, but
 *   can also still read through the whole thing fairly easily.
 *   
 *   To avoid creating an unnecessary dependency on the menu subsystem for
 *   games that don't want the menu-style instructions, we'll only define
 *   the menu objects if the preprocessor symbol INSTRUCTIONS_MENU is
 *   defined.  So, if you want to use the menu-style instructions, just
 *   add -D INSTRUCTIONS_MENU to your project makefile.  
 */
#ifdef INSTRUCTIONS_MENU

/* a base class for the instruction chapter menus */
class InstructionsMenu: MenuLongTopicItem
    /* 
     *   present the instructions in "chapter" format, so that we can
     *   navigate from one chapter directly to the next 
     */
    isChapterMenu = true

    /* the InstructionsAction property that we invoke to show our chapter */
    chapterProp = nil

    /* don't use a heading, as we provide our own internal chapter titles */
    heading = ''

    /* show our chapter text */
    menuContents = (InstructionsAction.(self.chapterProp)())
;

InstructionsMenu template 'title' ->chapterProp;

/*
 *   The main instructions menu.  This can be used as a top-level menu;
 *   just call the 'display()' method on this object to display the menu.
 *   This can also be used as a sub-menu of any other menu, simply by
 *   inserting this menu into a parent menu's 'contents' list.  
 */
topInstructionsMenu: MenuItem 'How to Play Interactive Fiction';

/* the chapter menus */
+ MenuLongTopicItem
    isChapterMenu = true
    title = 'Introduction'
    heading = nil
    menuContents()
    {
        "\b<b>Introduction</b>
        \bWelcome!  If you&rsquo;ve never played Interactive Fiction
        before, these instructions are designed to help you
        get started.  If you already know how to play this kind
        of game, you can probably skip the full instructions, but
        you might want to type ABOUT at the command prompt for a
        summary of the special features of this story.
        \b
        To make the instructions easier to navigate, they&rsquo;re
        broken up into chapters. ";

        if (curKeyList != nil && curKeyList.length() > 0)
            "At the end of each chapter, just press
            <b><<curKeyList[M_SEL][1].toUpper()>></b> to proceed to
            the next chapter, or <b><<curKeyList[M_PREV][1].toUpper()>></b>
            to return to the chapter list. ";
        else
            "To flip through the chapters, click on the links or
            use the Left/Right arrow keys. ";
    }
;

+ InstructionsMenu 'Entering Commands' ->(&showCommandsChapter);
+ InstructionsMenu 'Command Abbreviations' ->(&showAbbrevChapter);
+ InstructionsMenu 'Travel' ->(&showTravelChapter);
+ InstructionsMenu 'Manipulating Objects' ->(&showObjectsChapter);
+ InstructionsMenu 'Interacting with Other Characters'
    ->(&showConversationChapter);
+ InstructionsMenu 'Time' ->(&showTimeChapter);
+ InstructionsMenu 'Saving and Restoring' ->(&showSaveRestoreChapter);
+ InstructionsMenu 'Other Special Commands' ->(&showSpecialCmdChapter);
+ InstructionsMenu 'Unknown Words' ->(&showUnknownWordsChapter);
+ InstructionsMenu 'Ambiguous Commands' ->(&showAmbiguousCmdChapter);
+ InstructionsMenu 'Advanced Command Formats' ->(&showAdvancedCmdChapter);
+ InstructionsMenu 'A Few Tips' ->(&showTipsChapter);

#endif /* INSTRUCTIONS_MENU */

