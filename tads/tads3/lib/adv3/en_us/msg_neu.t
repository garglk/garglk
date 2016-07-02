#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *
 *   TADS 3 Library - "neutral" messages for US English
 *
 *   This module provides standard library messages with a parser/narrator 
 *   that's as invisible (neutral) as possible.  These messages are designed 
 *   to reduce the presence of the computer as mediator in the story, to 
 *   give the player the most direct contact that we can with the scenario.
 *
 *   The parser almost always refers to itself in the third person (by 
 *   calling itself something like "this story") rather than in the first 
 *   person, and, whenever possible, avoids referring to itself in the first 
 *   place.  Our ideal phrasing is either second-person, describing things 
 *   directly in terms of the player character's experience, or "no-person," 
 *   simply describing things without mentioning the speaker or listener at 
 *   all.  For example, rather than saying "I don't see that here," we say 
 *   "you don't see that here," or "that's not here."  We occasionally stray 
 *   from this ideal where achieving it would be too awkward.
 *
 *   In the earliest days of adventure games, the parser was usually a 
 *   visible presence: the early parsers frequently reported things in the 
 *   first person, and some even had specific personalities.  This 
 *   conspicuous parser style has become less prevalent in modern games, 
 *   though, and authors now usually prefer to treat the parser as just 
 *   another part of the user interface, which like all good UI's is best 
 *   when the user doesn't notice it.  
 */

#include "adv3.h"
#include "en_us.h"

/* ------------------------------------------------------------------------ */
/*
 *   Build a message parameter string with the given parameter type and
 *   name.
 *   
 *   This is useful when we have a name from a variable, and we need to
 *   build the message substitution string for embedding in a larger
 *   string.  We can't just embed the name variable using <<var>>, because
 *   that would process the output piecewise - the output filter needs to
 *   see the whole {typ var} expression in one go.  So, instead of writing
 *   this:
 *   
 *.     {The/he <<var>>} {is} ...
 *   
 *   write this:
 *   
 *.     <<buildParam('The/he', var)>> {is} ...
 */
buildParam(typeString, nm)
{
    return '{' + typeString + ' ' + nm + '}';
}

/*
 *   Synthesize a message parameter, and build it into a parameter string
 *   with the given substitution type.
 *   
 *   For example, buildSynthParam('abc', obj) returns '{abc xxx}', where
 *   'xxx' is a synthesized message parameter name (created using
 *   gSynthMessageParam) for the object obj.  
 */
buildSynthParam(typeString, obj)
{
    return '{' + typeString + ' ' + gSynthMessageParam(obj) + '}';
}


/* ------------------------------------------------------------------------ */
/*
 *   Library Messages 
 */
libMessages: MessageHelper
    /*
     *   The pronoun to use for the objective form of the personal
     *   interrogative pronoun.  Strictly speaking, the right word for
     *   this usage is "whom"; but regardless of what the grammar books
     *   say, most American English speakers these days use "who" for both
     *   the subjective and objective cases; to many ears, "whom" sounds
     *   old-fashioned, overly formal, or even pretentious.  (Case in
     *   point: a recent television ad tried to make a little kid look
     *   ultra-sophisticated by having him answer the phone by asking
     *   "*whom* may I ask is calling?", with elaborate emphasis on the
     *   "whom."  Of course, the correct usage in this case is "who," so
     *   the ad only made the kid look pretentious.  It goes to show that,
     *   at least in the mind of the writer of the ad, "whom" is just the
     *   snooty, formal version of "who" that serves only to signal the
     *   speaker's sophistication.)
     *   
     *   By default, we distinguish "who" and "whom."  Authors who prefer
     *   to use "who" everywhere can do so by changing this property's
     *   value to 'who'.  
     */
    whomPronoun = 'whom'

    /*
     *   Flag: offer an explanation of the "OOPS" command when it first
     *   comes up.  We'll only show this the first time the player enters
     *   an unknown word.  If you never want to offer this message at all,
     *   simply set this flag to nil initially.
     *   
     *   See also oopsNote() below.  
     */
    offerOopsNote = true

    /*
     *   some standard commands for insertion into <a> tags - these are in
     *   the messages so they can translated along with the command set
     */
    commandLookAround = 'look around'
    commandFullScore = 'full score'
    
    /* announce a completely remapped action */
    announceRemappedAction(action)
    {
        return '<./p0>\n<.assume>' + action.getParticiplePhrase()
            + '<./assume>\n';
    }

    /*
     *   Get a string to announce an implicit action.  This announces the
     *   current global action.  'ctx' is an ImplicitAnnouncementContext
     *   object describing the context in which the message is being
     *   displayed.  
     */
    announceImplicitAction(action, ctx)
    {
        /* build the announcement from the basic verb phrase */
        return ctx.buildImplicitAnnouncement(action.getImplicitPhrase(ctx));
    }

    /*
     *   Announce a silent implied action.  This allows an implied action
     *   to work exactly as normal (including the suppression of a default
     *   response message), but without any announcement of the implied
     *   action. 
     */
    silentImplicitAction(action, ctx) { return ''; }

    /*
     *   Get a string to announce that we're implicitly moving an object to
     *   a bag of holding to make room for taking something new.  If
     *   'trying' is true, it means we want to phrase the message as merely
     *   trying the action, not actually performing it.  
     */
    announceMoveToBag(action, ctx)
    {
        /* build the announcement, adding an explanation */
        return ctx.buildImplicitAnnouncement(
            action.getImplicitPhrase(ctx) + ' to make room');
    }

    /* show a library credit (for a CREDITS listing) */
    showCredit(name, byline) { "<<name>> <<byline>>"; }

    /* show a library version number (for a VERSION listing) */
    showVersion(name, version) { "<<name>> version <<version>>"; }

    /* there's no "about" information in this game */
    noAboutInfo = "<.parser>This story has no ABOUT information.<./parser> "

    /*
     *   Show a list state name - this is extra state information that we
     *   show for an object in a listing involving the object.  For
     *   example, a light source might add a state like "(providing
     *   light)".  We simply show the list state name in parentheses.  
     */
    showListState(state) { " (<<state>>)"; }

    /* a set of equivalents are all in a given state */
    allInSameListState(lst, stateName)
        { " (<<lst.length() == 2 ? 'both' : 'all'>> <<stateName>>)"; }

    /* generic long description of a Thing from a distance */
    distantThingDesc(obj)
    {
        gMessageParams(obj);
        "{It's obj} too far away to make out any detail. ";
    }

    /* generic long description of a Thing under obscured conditions */
    obscuredThingDesc(obj, obs)
    {
        gMessageParams(obj, obs);
        "{You/he} {can\'t} make out any detail through {the obs/him}. ";
    }

    /* generic "listen" description of a Thing at a distance */
    distantThingSoundDesc(obj)
        { "{You/he} {can\'t} hear any detail from this distance. "; }

    /* generic obscured "listen" description */
    obscuredThingSoundDesc(obj, obs)
    {
        gMessageParams(obj, obs);
        "{You/he} {can\'t} hear any detail through {the obs/him}. ";
    }

    /* generic "smell" description of a Thing at a distance */
    distantThingSmellDesc(obj)
        { "{You/he} {can\'t} smell much at this distance. "; }

    /* generic obscured "smell" description */
    obscuredThingSmellDesc(obj, obs)
    {
        gMessageParams(obj, obs);
        "{You/he} {can\'t} smell much through {the obs/him}. ";
    }

    /* generic "taste" description of a Thing */
    thingTasteDesc(obj)
    {
        gMessageParams(obj);
        "{It/he obj} taste{s/d} much as {you/he} would {|have}
        expect{|ed}. ";
    }

    /* generic "feel" description of a Thing */
    thingFeelDesc(obj)
        { "{You/he} {feel[s]|felt} nothing out of the ordinary. "; }

    /* obscured "read" description */
    obscuredReadDesc(obj)
    {
        gMessageParams(obj);
        "{You/he} {can\'t} see {that obj/him} well enough to read
        {it/him}. ";
    }

    /* dim light "read" description */
    dimReadDesc(obj)
    {
        gMessageParams(obj);
        "There{&rsquo;s| was} not enough light to read {that obj/him}. ";
    }

    /* lit/unlit match description */
    litMatchDesc(obj) { "\^<<obj.nameIs>> lit. "; }
    unlitMatchDesc(obj) { "\^<<obj.nameIs>> an ordinary match. "; }

    /* lit candle description */
    litCandleDesc(obj) { "\^<<obj.nameIs>> lit. "; }

    /*
     *   Prepositional phrases for putting objects into different types of
     *   objects. 
     */
    putDestContainer(obj) { return 'into ' + obj.theNameObj; }
    putDestSurface(obj) { return 'onto ' + obj.theNameObj; }
    putDestUnder(obj) { return 'under ' + obj.theNameObj; }
    putDestBehind(obj) { return 'behind ' + obj.theNameObj; }
    putDestFloor(obj) { return 'to ' + obj.theNameObj; }
    putDestRoom(obj) { return 'into ' + obj.theNameObj; }

    /* the list separator character in the middle of a list */
    listSepMiddle = ", "

    /* the list separator character for a two-element list */
    listSepTwo = " and "

    /* the list separator for the end of a list of at least three elements */
    listSepEnd = ", and "

    /*
     *   the list separator for the middle of a long list (a list with
     *   embedded lists not otherwise set off, such as by parentheses) 
     */
    longListSepMiddle = "; "

    /* the list separator for a two-element list of sublists */
    longListSepTwo = ", and "

    /* the list separator for the end of a long list */
    longListSepEnd = "; and "

    /* show the basic score message */
    showScoreMessage(points, maxPoints, turns)
    {
        "In <<turns>> move<<turns == 1 ? '' : 's'>>, you have
        scored <<points>> of a possible <<maxPoints>> point<<
          maxPoints == 1 ? '' : 's'>>. ";
    }

    /* show the basic score message with no maximum */
    showScoreNoMaxMessage(points, turns)
    {
        "In <<turns>> move<<turns == 1 ? '' : 's'>>, you have
        scored <<points>> point<<points == 1 ? '' : 's'>>. ";
    }

    /* show the full message for a given score rank string */
    showScoreRankMessage(msg) { "This makes you <<msg>>. "; }

    /*
     *   show the list prefix for the full score listing; this is shown on
     *   a line by itself before the list of full score items, shown
     *   indented and one item per line 
     */
    showFullScorePrefix = "Your score consists of:"

    /*
     *   show the item prefix, with the number of points, for a full score
     *   item - immediately after this is displayed, we'll display the
     *   description message for the achievement 
     */
    fullScoreItemPoints(points)
    {
        "<<points>> point<<points == 1 ? '' : 's'>> for ";
    }

    /* score change - first notification */
    firstScoreChange(delta)
    {
        scoreChange(delta);
        scoreChangeTip.showTip();
    }

    /* score change - notification other than the first time */
    scoreChange(delta)
    {
        "<.commandsep><.notification><<
        basicScoreChange(delta)>><./notification> ";
    }

    /*
     *   basic score change notification message - this is an internal
     *   service routine for scoreChange and firstScoreChange 
     */
    basicScoreChange(delta)
    {
        "Your <<aHref(commandFullScore, 'score', 'Show full score')>>
        has just <<delta > 0 ? 'in' : 'de'>>creased by
        <<spellInt(delta > 0 ? delta : -delta)>>
        point<<delta is in (1, -1) ? '' : 's'>>.";
    }

    /* acknowledge turning tips on or off */
    acknowledgeTipStatus(stat)
    {
        "<.parser>Tips are now turned <<stat ? 'on' : 'off'>>.<./parser> ";
    }

    /* describe the tip mode setting */
    tipStatusShort(stat)
    {
        "TIPS <<stat ? 'ON' : 'OFF'>>";
    }

    /* get the string to display for a footnote reference */
    footnoteRef(num)
    {
        /* set up a hyperlink for the note that enters the "note n" command */
        return '<sup>[<<aHref('footnote ' + num, toString(num))>>]</sup>';
    }

    /* first footnote notification */
    firstFootnote()
    {
        footnotesTip.showTip();
    }

    /* there is no such footnote as the given number */
    noSuchFootnote(num)
    {
        "<.parser>The story has never referred to any such
        footnote.<./parser> ";
    }

    /* show the current footnote status */
    showFootnoteStatus(stat)
    {
        "The current setting is FOOTNOTES ";
        switch(stat)
        {
        case FootnotesOff:
            "OFF, which hides all footnote references.
            Type <<aHref('footnotes medium', 'FOOTNOTES MEDIUM',
                         'Set footnotes to Medium')>> to
            show references to footnotes except those you&rsquo;ve
            already seen, or <<aHref('footnotes full', 'FOOTNOTES FULL',
                                     'Set footnotes to Full')>>
            to show all footnote references. ";
            break;

        case FootnotesMedium:
            "MEDIUM, which shows references to unread footnotes, but
            hides references to those you&rsquo;ve already read.  Type
            <<aHref('footnotes off', 'FOOTNOTES OFF',
                    'Turn off footnotes')>> to hide
            footnote references entirely, or <<aHref(
                'footnotes full', 'FOOTNOTES FULL',
                'Set footnotes to Full')>> to show every reference, even to
            notes you&rsquo;ve already read. ";
            break;

        case FootnotesFull:
            "FULL, which shows every footnote reference, even to
            notes you&rsquo;ve already read.  Type <<aHref('footnotes medium',
            'FOOTNOTES MEDIUM', 'Set footnotes to Medium')>> to show
            only references to notes you
            haven&rsquo;t yet read, or <<
              aHref('footnotes off', 'FOOTNOTES OFF', 'Turn off footnotes')>>
            to hide footnote references entirely. ";
            break;
        }
    }

    /* acknowledge a change in the footnote status */
    acknowledgeFootnoteStatus(stat)
    {
        "<.parser>The setting is now
        <<shortFootnoteStatus(stat)>>.<./parser> ";
    }

    /* show the footnote status, in short form */
    shortFootnoteStatus(stat)
    {
        "FOOTNOTES <<
          stat == FootnotesOff ? 'OFF'
          : stat == FootnotesMedium ? 'MEDIUM'
          : 'FULL' >>";
    }

    /*
     *   Show the main command prompt.
     *   
     *   'which' is one of the rmcXxx phase codes indicating what kind of
     *   command we're reading.  This default implementation shows the
     *   same prompt for every type of input, but games can use the
     *   'which' value to show different prompts for different types of
     *   queries, if desired.  
     */
    mainCommandPrompt(which) { "\b&gt;"; }

    /*
     *   Show a pre-resolved error message string.  This simply displays
     *   the given string.  
     */
    parserErrorString(actor, msg) { say(msg); }

    /* show the response to an empty command line */
    emptyCommandResponse = "<.parser>I beg your pardon?<./parser> "

    /* invalid token (i.e., punctuation) in command line */
    invalidCommandToken(ch)
    {
        "<.parser>The story doesn&rsquo;t know how to use the character
        &lsquo;<<ch>>&rsquo; in a command.<./parser> ";
    }

    /*
     *   Command group prefix - this is displayed after a command line and
     *   before the first command results shown after the command line.
     *   
     *   By default, we'll show the "zero-space paragraph" marker, which
     *   acts like a paragraph break in that it swallows up immediately
     *   following paragraph breaks, but doesn't actually add any space.
     *   This will ensure that we don't add any space between the command
     *   input line and the next text.  
     */
    commandResultsPrefix = '<.p0>'

    /*
     *   Command "interruption" group prefix.  This is displayed after an
     *   interrupted command line - a command line editing session that
     *   was interrupted by a timeout event - just before the text that
     *   interrupted the command line.
     *   
     *   By default, we'll show a paragraph break here, to set off the
     *   interrupting text from the command line under construction.  
     */
    commandInterruptionPrefix = '<.p>'

    /*
     *   Command separator - this is displayed after the results from a
     *   command when another command is about to be executed without any
     *   more user input.  That is, when a command line contains more than
     *   one command, this message is displayed between each successive
     *   command, to separate the results visually.
     *   
     *   This is not shown before the first command results after a
     *   command input line, and is not shown after the last results
     *   before a new input line.  Furthermore, this is shown only between
     *   adjacent commands for which output actually occurs; if a series
     *   of commands executes without any output, we won't show any
     *   separators between the silent commands.
     *   
     *   By default, we'll just start a new paragraph.  
     */
    commandResultsSeparator = '<.p>'

    /*
     *   "Complex" result separator - this is displayed between a group of
     *   messages for a "complex" result set and adjoining messages.  A
     *   command result list is "complex" when it's built up out of
     *   several generated items, such as object identification prefixes
     *   or implied command prefixes.  We use additional visual separation
     *   to set off these groups of messages from adjoining messages,
     *   which is especially important for commands on multiple objects,
     *   where we would otherwise have several results shown together.  By
     *   default, we use a paragraph break.  
     */
    complexResultsSeparator = '<.p>'

    /*
     *   Internal results separator - this is displayed to visually
     *   separate the results of an implied command from the results for
     *   the initiating command, which are shown after the results from
     *   the implied command.  By default, we show a paragraph break.
     */
    internalResultsSeparator = '<.p>'

    /*
     *   Command results suffix - this is displayed just before a new
     *   command line is about to be read if any command results have been
     *   shown since the last command line.
     *   
     *   By default, we'll show nothing extra.  
     */
    commandResultsSuffix = ''

    /*
     *   Empty command results - this is shown when we read a command line
     *   and then go back and read another without having displaying
     *   anything.
     *   
     *   By default, we'll return a message indicating that nothing
     *   happened.  
     */
    commandResultsEmpty =
        ('Nothing obvious happen' + tSel('s', 'ed') + '.<.p>')

    /*
     *   Intra-command report separator.  This is used to separate report
     *   messages within a single command's results.  By default, we show
     *   a paragraph break.  
     */
    intraCommandSeparator = '<.p>'

    /*
     *   separator for "smell" results - we ordinarily show each item's
     *   odor description as a separate paragraph 
     */
    smellDescSeparator()
    {
        "<.p>";
    }

    /*
     *   separator for "listen" results 
     */
    soundDescSeparator()
    {
        "<.p>";
    }

    /* a command was issued to a non-actor */
    cannotTalkTo(targetActor, issuingActor)
    {
        "\^<<targetActor.nameIs>> not something <<issuingActor.itNom>>
        <<issuingActor.verbCan>> talk to. ";
    }

    /* greeting actor while actor is already talking to us */
    alreadyTalkingTo(actor, greeter)
    {
        "\^<<greeter.theName>> already <<greeter.verbToHave>>
        <<actor.theNamePossAdj>> attention. ";
    }

    /* no topics to suggest when we're not talking to anyone */
    noTopicsNotTalking = "<.parser>{You're} not currently talking
                          to anyone.<./parser> "

    /*
     *   Show a note about the OOPS command.  This is, by default, added
     *   to the "I don't know that word" error the first time that error
     *   occurs.  
     */
    oopsNote()
    {
        oopsTip.showTip();
    }

    /* can't use OOPS right now */
    oopsOutOfContext = "<.parser>You can only use OOPS to correct
        a misspelling immediately after the story points out a word
        it doesn&rsquo;t know.<./parser> "

    /* OOPS in context, but without the word to correct */
    oopsMissingWord = "<.parser>To use OOPS to correct a misspelling,
        put the corrected word after OOPS, as in OOPS BOOK.<./parser> "

    /* acknowledge setting VERBOSE mode (true) or TERSE mode (nil) */
    acknowledgeVerboseMode(verbose)
    {
        if (verbose)
            "<.parser>VERBOSE mode is now selected.<./parser> ";
        else
            "<.parser>TERSE mode is now selected.<./parser> ";
    }

    /* show the current VERBOSE setting, in short form */
    shortVerboseStatus(stat) { "<<stat ? 'VERBOSE' : 'TERSE'>> mode"; }

    /* show the current score notify status */
    showNotifyStatus(stat)
    {
        "<.parser>Score notifications are
        currently <<stat ? 'on' : 'off'>>.<./parser> ";
    }

    /* show the current score notify status, in short form */
    shortNotifyStatus(stat) { "NOTIFY <<stat ? 'ON' : 'OFF'>>"; }

    /* acknowledge a change in the score notification status */
    acknowledgeNotifyStatus(stat)
    {
        "<.parser>Score notifications are now
        <<stat ? 'on' : 'off'>>.<./parser> ";
    }

    /*
     *   Announce the current object of a set of multiple objects on which
     *   we're performing an action.  This is used to tell the player
     *   which object we're acting upon when we're iterating through a set
     *   of objects specified in a command targeting multiple objects.  
     */
    announceMultiActionObject(obj, whichObj, action)
    {
        /* 
         *   get the display name - we only need to differentiate this
         *   object from the other objects in the iteration 
         */
        local nm = obj.getAnnouncementDistinguisher(
            action.getResolvedObjList(whichObj)).name(obj);

        /* build the announcement */
        return '<./p0>\n<.announceObj>' + nm + ':<./announceObj> <.p0>';
    }

    /*
     *   Announce a singleton object that we selected from a set of
     *   ambiguous objects.  This is used when we disambiguate a command
     *   and choose an object over other objects that are also logical but
     *   are less likely.  In such cases, it's courteous to tell the
     *   player what we chose, because it's possible that the user meant
     *   one of the other logical objects - announcing this type of choice
     *   helps reduce confusion by making it immediately plain to the
     *   player when we make a choice other than what they were thinking.  
     */
    announceAmbigActionObject(obj, whichObj, action)
    {
        /* 
         *   get the display name - distinguish the object from everything
         *   else in scope, since we chose from a set of ambiguous options 
         */
        local nm = obj.getAnnouncementDistinguisher(gActor.scopeList())
            .theName(obj);

        /* announce the object in "assume" style, ending with a newline */
        return '<.assume>' + nm + '<./assume>\n';
    }

    /*
     *   Announce a singleton object we selected as a default for a
     *   missing noun phrase.
     *   
     *   'resolvedAllObjects' indicates where we are in the command
     *   processing: this is true if we've already resolved all of the
     *   other objects in the command, nil if not.  We use this
     *   information to get the phrasing right according to the situation.
     */
    announceDefaultObject(obj, whichObj, action, resolvedAllObjects)
    {
        /*
         *   put the action's default-object message in "assume" style,
         *   and start a new line after it 
         */
        return '<.assume>'
            + action.announceDefaultObject(obj, whichObj, resolvedAllObjects)
            + '<./assume>\n';
    }

    /* 'again' used with no prior command */
    noCommandForAgain()
    {
        "<.parser>There&rsquo;s nothing to repeat.<./parser> ";
    }

    /* 'again' cannot be directed to a different actor */
    againCannotChangeActor()
    {
        "<.parser>To repeat a command like <q>turtle, go north,</q>
        just say <q>again,</q> not <q>turtle, again.</q><./parser> ";
    }

    /* 'again': can no longer talk to target actor */
    againCannotTalkToTarget(issuer, target)
    {
        "\^<<issuer.theName>> <<issuer.verbCannot>> repeat that command. ";
    }

    /* the last command cannot be repeated in the present context */
    againNotPossible(issuer)
    {
        "That command cannot be repeated now. ";
    }

    /* system actions cannot be directed to non-player characters */
    systemActionToNPC()
    {
        "<.parser>This command cannot be directed to another
        character in the story.<./parser> ";
    }

    /* confirm that we really want to quit */
    confirmQuit()
    {
        "Do you really want to quit?\ (<<aHref('y', 'Y', 'Confirm quitting')
        >> is affirmative) >\ ";
    }

    /*
     *   QUIT message.  We display this to acknowledge an explicit player
     *   command to quit the game.  This is the last message the game
     *   displays on the way out; there is no need to offer any options at
     *   this point, because the player has decided to exit the game.
     *   
     *   By default, we show nothing; games can override this to display an
     *   acknowledgment if desired.  Note that this isn't a general
     *   end-of-game 'goodbye' message; the library only shows this to
     *   acknowledge an explicit QUIT command from the player.  
     */
    okayQuitting() { }

    /*
     *   "not terminating" confirmation - this is displayed when the
     *   player doesn't acknowledge a 'quit' command with an affirmative
     *   response to our confirmation question 
     */
    notTerminating()
    {
        "<.parser>Continuing the story.<./parser> ";
    }

    /* confirm that they really want to restart */
    confirmRestart()
    {
        "Do you really want to start over?\ (<<aHref('Y', 'Y',
        'Confirm restart')>> is affirmative) >\ ";
    }

    /* "not restarting" confirmation */
    notRestarting() { "<.parser>Continuing the story.<./parser> "; }

    /*
     *   Show a game-finishing message - we use the conventional "*** You
     *   have won! ***" format that text games have been using since the
     *   dawn of time. 
     */
    showFinishMsg(msg) { "<.p>*** <<msg>>\ ***<.p>"; }

    /* standard game-ending messages for the common outcomes */
    finishDeathMsg = '{YOU/HE pc} {[HAVE]|} DIED'
    finishVictoryMsg = ('YOU ' + tSel('HAVE ', '') + 'WON')
    finishFailureMsg = ('YOU ' + tSel('HAVE ', '') + 'FAILED')
    finishGameOverMsg = 'GAME OVER'

    /*
     *   Get the save-game file prompt.  Note that this must return a
     *   single-quoted string value, not display a value itself, because
     *   this prompt is passed to inputFile(). 
     */
    getSavePrompt = 'Save game to file'

    /* get the restore-game prompt */
    getRestorePrompt = 'Restore game from file'

    /* successfully saved */
    saveOkay() { "<.parser>Saved.<./parser> "; }

    /* save canceled */
    saveCanceled() { "<.parser>Canceled.<./parser> "; }

    /* saved failed due to a file write or similar error */
    saveFailed(exc)
    {
        "<.parser>Failed; your computer might be running low
        on disk space, or you might not have the necessary permissions
        to write this file.<./parser> ";
    }

    /* save failed due to storage server request error */
    saveFailedOnServer(exc)
    {
        "<.parser>Failed, because of a problem accessing the storage server:
        <<makeSentence(exc.errMsg)>><./parser>";
    }

    /* 
     *   make an error message into a sentence, by capitalizing the first
     *   letter and adding a period at the end if it doesn't already have
     *   one 
     */
    makeSentence(msg)
    {
        return rexReplace(
            ['^<space>*[a-z]', '(?<=[^.?! ])<space>*$'], msg,
            [{m: m.toUpper()}, '.']);
    }

    /* note that we're restoring at startup via a saved-position launch */
    noteMainRestore() { "<.parser>Restoring saved game...<./parser>\n"; }

    /* successfully restored */
    restoreOkay() { "<.parser>Restored.<./parser> "; }

    /* restore canceled */
    restoreCanceled() { "<.parser>Canceled.<./parser> "; }

    /* restore failed due to storage server request error */
    restoreFailedOnServer(exc)
    {
        "<.parser>Failed, because of a problem accessing the storage server:
        <<makeSentence(exc.errMsg)>><./parser>";
    }

    /* restore failed because the file was not a valid saved game file */
    restoreInvalidFile()
    {
        "<.parser>Failed: this is not a valid saved
        position file.<./parser> ";
    }

    /* restore failed because the file was corrupted */
    restoreCorruptedFile()
    {
        "<.parser>Failed: this saved state file appears to be
        corrupted.  This can occur if the file was modified by another
        program, or the file was copied between computers in a non-binary
        transfer mode, or the physical media storing the file were
        damaged.<./parser> ";
    }

    /* restore failed because the file was for the wrong game or version */
    restoreInvalidMatch()
    {
        "<.parser>Failed: the file was not saved by this
        story (or was saved by an incompatible version of
        the story).<./parser> ";
    }

    /* restore failed for some reason other than those distinguished above */
    restoreFailed(exc)
    {
        "<.parser>Failed: the position could not be
        restored.<./parser> ";
    }

    /* error showing the input file dialog (or character-mode equivalent) */
    filePromptFailed()
    {
        "<.parser>A system error occurred asking for a filename.
        Your computer might be running low on memory, or might have a
        configuration problem.<./parser> ";
    }

    /* error showing the input file dialog, with a system error message */
    filePromptFailedMsg(msg)
    {
        "<.parser>Failed: <<makeSentence(msg)>><./parser> ";
    }

    /* Web UI inputFile error: uploaded file is too large */
    webUploadTooBig = 'The file you selected is too large to upload.'

    /* PAUSE prompt */
    pausePrompt()
    {
        "<.parser>The story is now paused.  Please press
        the space bar when you are ready to resume the story, or
        press the &lsquo;S&rsquo; key to save the current
        position.<./parser><.p>";
    }

    /* saving from within a pause */
    pauseSaving()
    {
        "<.parser>Saving the story...<./parser><.p>";
    }

    /* PAUSE ended */
    pauseEnded()
    {
        "<.parser>Resuming the story.<./parser> ";
    }

    /* acknowledge starting an input script */
    inputScriptOkay(fname)
    {
        "<.parser>Reading commands from <q><<
          File.getRootName(fname).htmlify()>></q>...<./parser>\n ";
    }

    /* error opening input script */
    inputScriptFailed = "<.parser>Failed; the script input file could
        not be opened.<./parser> "
        
    /* exception opening input script */
    inputScriptFailedException(exc)
    {
        "<.parser>Failed; <<exc.displayException>><./parser> ";
    }

    /* get the scripting inputFile prompt message */
    getScriptingPrompt = 'Please select a name for the new script file'

    /* acknowledge scripting on */
    scriptingOkay()
    {
        "<.parser>The transcript will be saved to the file.
        Type <<aHref('script off', 'SCRIPT OFF', 'Turn off scripting')>> to
        discontinue scripting.<./parser> ";
    }

    scriptingOkayWebTemp()
    {
        "<.parser>The transcript will be saved.
        Type <<aHref('script off', 'SCRIPT OFF', 'Turn off scripting')>>
        to discontinue scripting and download the saved
        transcript.<./parser> ";
    }

    /* scripting failed */
    scriptingFailed = "<.parser>Failed; an error occurred opening
        the script file.<./parser> "

    /* scripting failed with an exception */
    scriptingFailedException(exc)
    {
        "<.parser>Failed; <<exc.displayException>><./parser>";
    }

    /* acknowledge cancellation of script file dialog */
    scriptingCanceled = "<.parser>Canceled.<./parser> "

    /* acknowledge scripting off */
    scriptOffOkay = "<.parser>Scripting ended.<./parser> "

    /* SCRIPT OFF ignored because we're not in a script file */
    scriptOffIgnored = "<.parser>No script is currently being
                        recorded.<./parser> "

    /* get the RECORD prompt */
    getRecordingPrompt = 'Please select a name for the new command log file'

    /* acknowledge recording on */
    recordingOkay = "<.parser>Commands will now be recorded.  Type
                     <<aHref('record off', 'RECORD OFF',
                             'Turn off recording')>>
                     to stop recording commands.<.parser> "

    /* recording failed */
    recordingFailed = "<.parser>Failed; an error occurred opening
        the command recording file.<./parser> "

    /* recording failed with exception */
    recordingFailedException(exc)
    {
        "<.parser>Failed; <<exc.displayException()>><./parser> ";
    }

    /* acknowledge cancellation */
    recordingCanceled = "<.parser>Canceled.<./parser> "

    /* recording turned off */
    recordOffOkay = "<.parser>Command recording ended.<./parser> "

    /* RECORD OFF ignored because we're not recording commands */
    recordOffIgnored = "<.parser>No command recording is currently being
                        made.<./parser> "

    /* REPLAY prompt */
    getReplayPrompt = 'Please select the command log file to replay'

    /* REPLAY file selection canceled */
    replayCanceled = "<.parser>Canceled.<./parser> "

    /* undo command succeeded */
    undoOkay(actor, cmd)
    {
        "<.parser>Taking back one turn: <q>";

        /* show the target actor prefix, if an actor was specified */
        if (actor != nil)
            "<<actor>>, ";

        /* show the command */
        "<<cmd>></q>.<./parser><.p>";
    }

    /* undo command failed */
    undoFailed()
    {
        "<.parser>No more undo information is
        available.<./parser> ";
    }

    /* comment accepted, with or without transcript recording in effect */
    noteWithScript = "<.parser>Comment recorded.<./parser> "
    noteWithoutScript = "<.parser>Comment <b>not</b> recorded.<./parser> "

    /* on the first comment without transcript recording, warn about it */
    noteWithoutScriptWarning = "<.parser>Comment <b>not</b> recorded.
        Use <<aHref('script', 'SCRIPT', 'Begin saving the transcript')
          >> if you want to start recording the transcript.<./parser> "

    /* invalid finishGame response */
    invalidFinishOption(resp)
    {
        "\bThat isn&rsquo;t one of the options. ";
    }

    /* acknowledge new "exits on/off" status */
    exitsOnOffOkay(stat, look)
    {
        if (stat && look)
            "<.parser>The list of exits will now be shown in both
            the status line and in each room description.<./parser> ";
        else if (!stat && !look)
            "<.parser>The list of exits will no longer be shown in
            either the status line or room description.<./parser> ";
        else
            "<.parser>The list of exits <<stat ? 'will' : 'will not'>> be
            shown in the status line, and <<look ? 'will' : 'won&rsquo;t'>>
            be included in room descriptions.<./parser> ";
    }

    /* explain how to turn exit display on and off */
    explainExitsOnOff()
    {
        exitsTip.showTip();
    }

    /* describe the current EXITS settings */
    currentExitsSettings(statusLine, roomDesc)
    {
        "EXITS ";
        if (statusLine && roomDesc)
            "ON";
        else if (statusLine)
            "STATUSLINE";
        else if (roomDesc)
            "LOOK";
        else
            "OFF";
    }

    /* acknowledge HINTS OFF */
    hintsDisabled = '<.parser>Hints are now disabled.<./parser> '

    /* rebuff a request for hints when they've been previously disabled */
    sorryHintsDisabled = '<.parser>Sorry, but hints have been disabled
        for this session, as you requested.  If you&rsquo;ve changed your
        mind, you&rsquo;ll have to save your current position, exit the TADS
        interpreter, and start a new interpreter session.<./parser> '

    /* this game has no hints */
    hintsNotPresent = '<.parser>Sorry, this story doesn&rsquo;t
                       have any built-in hints.<./parser> '

    /* there are currently no hints available (but there might be later) */
    currentlyNoHints = '<.parser>Sorry, no hints are currently available.
                        Please check back later.<./parser> '

    /* show the hint system warning */
    showHintWarning =
       "<.notification>Warning: Some people don&rsquo;t like built-in hints,
       since the temptation to ask for help prematurely can become
       overwhelming when hints are so close at hand.  If you&rsquo;re worried
       that your willpower won&rsquo;t hold up, you can disable hints for the
       rest of this session by typing <<aHref('hints off', 'HINTS OFF')
       >>.  If you still want to see the hints now, type
       <<aHref('hint', 'HINT')>>.<./notification> "

    /* done with hints */
    hintsDone = '<.parser>Done.<./parser> '

    /* optional command is not supported in this game */
    commandNotPresent = "<.parser>That command isn&rsquo;t needed
                         in this story.<./parser> "

    /* this game doesn't use scoring */
    scoreNotPresent = "<.parser>This story doesn&rsquo;t use
                       scoring.<./parser> "

    /* mention the FULL SCORE command */
    mentionFullScore()
    {
        fullScoreTip.showTip();
    }

    /* SAVE DEFAULTS successful */
    savedDefaults()
    {
        "<.parser>Your current settings have been stored as the
        default settings for new games.  The saved settings are: ";

        /* show all of the settings */
        settingsUI.showAll();

        ".  Most newer games will apply these settings automatically
        whenever you start (or RESTART) the game, but note that older
        games will not.<./parser> ";
    }

    /* RESTORE DEFAULTS successful */
    restoredDefaults()
    {
        "<.parser>The saved default settings have been put into
        effect.  The new settings are: ";

        /* show all of the settings */
        settingsUI.showAll();

        ".<./parser> ";
    }

    /* show a separator for the settingsUI.showAll() list */
    settingsItemSeparator = "; "

    /* SAVE/RESTORE DEFAULTS not supported (old interpreter version) */
    defaultsFileNotSupported = "<.parser>Sorry, but the version of the
        TADS interpreter you&rsquo;re using doesn&rsquo;t support saving
        or restoring defaults.  You must install a more recent version
        in order to use this feature.<./parser> "

    /* RESTORE DEFAULTS file open/read error */
    defaultsFileReadError(exc)
    {
        "<.parser>An error occurred reading the default settings
        file.  The global defaults can't be restored.<./parser> ";
    }

    /* SAVE DEFAULTS file creation error */
    defaultsFileWriteError = "<.parser>An error occurred writing
        the default settings file.  The defaults have not been
        saved.  You might be out of disk space, or you might not
        have the necessary permissions to write the file.<./parser> "

    /*
     *   Command key list for the menu system.  This uses the format
     *   defined for MenuItem.keyList in the menu system.  Keys must be
     *   given as lower-case in order to match input, since the menu
     *   system converts input keys to lower case before matching keys to
     *   this list.  
     *   
     *   Note that the first item in each list is what will be given in
     *   the navigation menu, which is why the fifth list contains 'ENTER'
     *   as its first item, even though this will never match a key press.
     */
    menuKeyList = [
                   ['q'],
                   ['p', '[left]', '[bksp]', '[esc]'],
                   ['u', '[up]'],
                   ['d', '[down]'],
                   ['ENTER', '\n', '[right]', ' ']
                  ]

    /* link title for 'previous menu' navigation link */
    prevMenuLink = '<font size=-1>Previous</font>'

    /* link title for 'next topic' navigation link in topic lists */
    nextMenuTopicLink = '<font size=-1>Next</font>'

    /*
     *   main prompt text for text-mode menus - this is displayed each
     *   time we ask for a keystroke to navigate a menu in text-only mode 
     */
    textMenuMainPrompt(keylist)
    {
        "\bSelect a topic number, or press &lsquo;<<
        keylist[M_PREV][1]>>&rsquo; for the previous
        menu or &lsquo;<<keylist[M_QUIT][1]>>&rsquo; to quit:\ ";
    }

    /* prompt text for topic lists in text-mode menus */
    textMenuTopicPrompt()
    {
        "\bPress the space bar to display the next line,
        &lsquo;<b>P</b>&rsquo; to go to the previous menu, or
        &lsquo;<b>Q</b>&rsquo; to quit.\b";
    }

    /*
     *   Position indicator for topic list items - this is displayed after
     *   a topic list item to show the current item number and the total
     *   number of items in the list, to give the user an idea of where
     *   they are in the overall list.  
     */
    menuTopicProgress(cur, tot) { " [<<cur>>/<<tot>>]"; }

    /*
     *   Message to display at the end of a topic list.  We'll display
     *   this after we've displayed all available items from a
     *   MenuTopicItem's list of items, to let the user know that there
     *   are no more items available.  
     */
    menuTopicListEnd = '[The End]'

    /*
     *   Message to display at the end of a "long topic" in the menu
     *   system.  We'll display this at the end of the long topic's
     *   contents.  
     */
    menuLongTopicEnd = '[The End]'

    /*
     *   instructions text for banner-mode menus - this is displayed in
     *   the instructions bar at the top of the screen, above the menu
     *   banner area 
     */
    menuInstructions(keylist, prevLink)
    {
        "<tab align=right ><b>\^<<keylist[M_QUIT][1]>></b>=Quit <b>\^<<
        keylist[M_PREV][1]>></b>=Previous Menu<br>
        <<prevLink != nil ? aHrefAlt('previous', prevLink, '') : ''>>
        <tab align=right ><b>\^<<keylist[M_UP][1]>></b>=Up <b>\^<<
        keylist[M_DOWN][1]>></b>=Down <b>\^<<
        keylist[M_SEL][1]>></b>=Select<br>";
    }

    /* show a 'next chapter' link */
    menuNextChapter(keylist, title, hrefNext, hrefUp)
    {
        "Next: <<aHref(hrefNext, title)>>;
        <b>\^<<keylist[M_PREV][1]>></b>=<<aHref(hrefUp, 'Menu')>>";
    }

    /*
     *   cannot reach (i.e., touch) an object that is to be manipulated in
     *   a command - this is a generic message used when we cannot
     *   identify the specific reason that the object is in scope but
     *   cannot be touched 
     */
    cannotReachObject(obj)
    {
        "{You/he} {cannot} reach <<obj.theNameObj>>. ";
    }

    /*
     *   cannot reach an object, because the object is inside the given
     *   container 
     */
    cannotReachContents(obj, loc)
    {
        gMessageParams(obj, loc);
        return '{You/he} {cannot} reach {the obj/him} through '
               + '{the loc/him}. ';
    }

    /* cannot reach an object because it's outisde the given container */
    cannotReachOutside(obj, loc)
    {
        gMessageParams(obj, loc);
        return '{You/he} {cannot} reach {the obj/him} through '
               + '{the loc/him}. ';
    }

    /* sound is coming from inside/outside a container */
    soundIsFromWithin(obj, loc)
    {
        "\^<<obj.theName>> appear<<obj.verbEndingSEd>> to be
        coming from inside <<loc.theNameObj>>. ";
    }
    soundIsFromWithout(obj, loc)
    {
        "\^<<obj.theName>> appear<<obj.verbEndingSEd>> to be
        coming from outside <<loc.theNameObj>>. ";
    }

    /* odor is coming from inside/outside a container */
    smellIsFromWithin(obj, loc)
    {
        "\^<<obj.theName>> appear<<obj.verbEndingSEd>> to be
        coming from inside <<loc.theNameObj>>. ";
    }
    smellIsFromWithout(obj, loc)
    {
        "\^<<obj.theName>> appear<<obj.verbEndingSEd>> to be
        coming from outside <<loc.theNameObj>>. ";
    }

    /* default description of the player character */
    pcDesc(actor)
    {
        "\^<<actor.theName>> look<<actor.verbEndingSEd>> the same
        as usual. ";
    }

    /*
     *   Show a status line addendum for the actor posture, without
     *   mentioning the actor's location.  We won't mention standing, since
     *   this is the default posture.  
     */
    roomActorStatus(actor)
    {
        /* mention any posture other than standing */
        if (actor.posture != standing)
            " (<<actor.posture.participle>>)";
    }

    /* show a status line addendum: standing in/on something */
    actorInRoomStatus(actor, room)
        { " (<<actor.posture.participle>> <<room.actorInName>>)"; }

    /* generic short description of a dark room */
    roomDarkName = 'In the dark'

    /* generic long description of a dark room */
    roomDarkDesc = "It{&rsquo;s| was} pitch black. "

    /*
     *   mention that an actor is here, without mentioning the enclosing
     *   room, as part of a room description 
     */
    roomActorHereDesc(actor)
    {
        "\^<<actor.nameIs>> <<actor.posture.participle>>
        <<tSel('here', 'there')>>. ";
    }

    /*
     *   mention that an actor is visible at a distance or remotely,
     *   without mentioning the enclosing room, as part of a room
     *   description 
     */
    roomActorThereDesc(actor)
    {
        "\^<<actor.nameIs>> <<actor.posture.participle>> nearby. ";
    }

    /*
     *   Mention that an actor is in a given local room, as part of a room
     *   description.  This is used as a default "special description" for
     *   an actor.  
     */
    actorInRoom(actor, cont)
    {
        "\^<<actor.nameIs>> <<actor.posture.participle>>
        <<cont.actorInName>>. ";
    }

    /*
     *   Describe an actor as standing/sitting/lying on something, as part
     *   of the actor's EXAMINE description.  This is additional
     *   information added to the actor's description, so we refer to the
     *   actor with a pronoun ("He's standing here").  
     */
    actorInRoomPosture(actor, room)
    {
        "\^<<actor.itIs>> <<actor.posture.participle>>
        <<room.actorInName>>. ";
    }

    /*
     *   Describe an actor's posture, as part of an actor's "examine"
     *   description.  If the actor is standing, don't bother mentioning
     *   anything, as standing is the trivial default condition.  
     */
    roomActorPostureDesc(actor)
    {
        if (actor.posture != standing)
            "\^<<actor.itIs>> <<actor.posture.participle>>. ";
    }

    /*
     *   mention that the given actor is visible, at a distance or
     *   remotely, in the given location; this is used in room
     *   descriptions when an NPC is visible in a remote or distant
     *   location 
     */
    actorInRemoteRoom(actor, room, pov)
    {
        /* say that the actor is in the room, using its remote in-name */
        "\^<<actor.nameIs>> <<actor.posture.participle>>
        <<room.inRoomName(pov)>>. ";
    }

    /*
     *   mention that the given actor is visible, at a distance or
     *   remotely, in the given nested room within the given outer
     *   location; this is used in room descriptions 
     */
    actorInRemoteNestedRoom(actor, inner, outer, pov)
    {
        /*
         *   say that the actor is in the nested room, in the current
         *   posture, and add then add that we're in the outer room as
         *   well 
         */
        "\^<<actor.nameIs>> <<outer.inRoomName(pov)>>,
        <<actor.posture.participle>> <<inner.actorInName>>. ";
    }

    /*
     *   Prefix/suffix messages for listing actors in a room description,
     *   for cases when the actors are in the local room in a nominal
     *   container that we want to mention: "Bob and Bill are sitting on
     *   the couch."  
     */
    actorInGroupPrefix(posture, cont, lst) { "\^"; }
    actorInGroupSuffix(posture, cont, lst)
    {
        " <<lst.length() == 1 ? tSel('is', 'was') : tSel('are', 'were')>>
        <<posture.participle>> <<cont.actorInName>>. ";
    }

    /*
     *   Prefix/suffix messages for listing actors in a room description,
     *   for cases when the actors are inside a nested room that's inside
     *   a remote location: "Bob and Bill are in the courtyard, sitting on
     *   the bench." 
     */
    actorInRemoteGroupPrefix(pov, posture, cont, remote, lst) { "\^"; }
    actorInRemoteGroupSuffix(pov, posture, cont, remote, lst)
    {
        " <<lst.length() == 1 ? tSel('is', 'was') : tSel('are', 'were')>>
        <<remote.inRoomName(pov)>>, <<posture.participle>>
        <<cont.actorInName>>. ";
    }

    /*
     *   Prefix/suffix messages for listing actors in a room description,
     *   for cases when the actors' nominal container cannot be seen or is
     *   not to be stated: "Bob and Bill are standing here."
     *   
     *   Note that we don't always want to state the nominal container,
     *   even when it's visible.  For example, when actors are standing on
     *   the floor, we don't bother saying that they're on the floor, as
     *   that's stating the obvious.  The container will decide whether or
     *   not it wants to be included in the message; containers that don't
     *   want to be mentioned will use this form of the message.  
     */
    actorHereGroupPrefix(posture, lst) { "\^"; }
    actorHereGroupSuffix(posture, lst)
    {
        " <<lst.length() == 1 ? tSel('is', 'was') : tSel('are', 'were')>>
        <<posture.participle>> <<tSel('here', 'there')>>. ";
    }

    /*
     *   Prefix/suffix messages for listing actors in a room description,
     *   for cases when the actors' immediate container cannot be seen or
     *   is not to be stated, and the actors are in a remote location:
     *   "Bob and Bill are in the courtyard."  
     */
    actorThereGroupPrefix(pov, posture, remote, lst) { "\^"; }
    actorThereGroupSuffix(pov, posture, remote, lst)
    {
        " <<lst.length() == 1 ? tSel('is', 'was') : tSel('are', 'were')>>
        <<posture.participle>> <<remote.inRoomName(pov)>>. ";
    }

    /* a traveler is arriving, but not from a compass direction */
    sayArriving(traveler)
    {
        "\^<<traveler.travelerName(true)>> enter<<traveler.verbEndingSEd>>
        <<traveler.travelerLocName>>. ";
    }

    /* a traveler is departing, but not in a compass direction */
    sayDeparting(traveler)
    {
        "\^<<traveler.travelerName(nil)>> <<traveler.verbToLeave>>
        <<traveler.travelerLocName>>. ";
    }

    /*
     *   a traveler is arriving locally (staying within view throughout the
     *   travel, and coming closer to the PC) 
     */
    sayArrivingLocally(traveler, dest)
    {
        "\^<<traveler.travelerName(true)>> enter<<traveler.verbEndingSEd>>
        <<traveler.travelerLocName>>. ";
    }

    /*
     *   a traveler is departing locally (staying within view throughout
     *   the travel, and moving further away from the PC) 
     */
    sayDepartingLocally(traveler, dest)
    {
        "\^<<traveler.travelerName(true)>> <<traveler.verbToLeave>>
        <<traveler.travelerLocName>>. ";
    }

    /*
     *   a traveler is traveling remotely (staying within view through the
     *   travel, and moving from one remote top-level location to another) 
     */
    sayTravelingRemotely(traveler, dest)
    {
        "\^<<traveler.travelerName(true)>> <<traveler.verbToGo>> to
        <<traveler.travelerLocName>>. ";
    }

    /* a traveler is arriving from a compass direction */
    sayArrivingDir(traveler, dirName)
    {
        "\^<<traveler.travelerName(true)>> enter<<traveler.verbEndingSEd>>
        <<traveler.travelerRemoteLocName>> from the <<dirName>>. ";
    }

    /* a traveler is leaving in a given compass direction */
    sayDepartingDir(traveler, dirName)
    {
        local nm = traveler.travelerRemoteLocName;
        
        "\^<<traveler.travelerName(nil)>> <<traveler.verbToLeave>>
        to the <<dirName>><<nm != '' ? ' from ' + nm : ''>>. ";
    }
    
    /* a traveler is arriving from a shipboard direction */
    sayArrivingShipDir(traveler, dirName)
    {
        "\^<<traveler.travelerName(true)>> enter<<traveler.verbEndingSEd>>
        <<traveler.travelerRemoteLocName>> from <<dirName>>. ";
    }

    /* a traveler is leaving in a given shipboard direction */
    sayDepartingShipDir(traveler, dirName)
    {
        local nm = traveler.travelerRemoteLocName;
        
        "\^<<traveler.travelerName(nil)>> <<traveler.verbToLeave>>
        to <<dirName>><<nm != '' ? ' from ' + nm : ''>>. ";
    }

    /* a traveler is going aft */
    sayDepartingAft(traveler)
    {
        local nm = traveler.travelerRemoteLocName;
        
        "\^<<traveler.travelerName(nil)>> <<traveler.verbToGo>>
        aft<<nm != '' ? ' from ' + nm : ''>>. ";
    }

    /* a traveler is going fore */
    sayDepartingFore(traveler)
    {
        local nm = traveler.travelerRemoteLocName;

        "\^<<traveler.travelerName(nil)>> <<traveler.verbToGo>>
        forward<<nm != '' ? ' from ' + nm : ''>>. ";
    }

    /* a shipboard direction was attempted while not onboard a ship */
    notOnboardShip = "That direction {is|was}n&rsquo;t meaningful {|t}here. "

    /* a traveler is leaving via a passage */
    sayDepartingThroughPassage(traveler, passage)
    {
        "\^<<traveler.travelerName(nil)>> <<traveler.verbToLeave>>
        <<traveler.travelerRemoteLocName>> through <<passage.theNameObj>>. ";
    }

    /* a traveler is arriving via a passage */
    sayArrivingThroughPassage(traveler, passage)
    {
        "\^<<traveler.travelerName(true)>> enter<<traveler.verbEndingSEd>>
        <<traveler.travelerRemoteLocName>> through <<passage.theNameObj>>. ";
    }

    /* a traveler is leaving via a path */
    sayDepartingViaPath(traveler, passage)
    {
        "\^<<traveler.travelerName(nil)>> <<traveler.verbToLeave>>
        <<traveler.travelerRemoteLocName>> via <<passage.theNameObj>>. ";
    }

    /* a traveler is arriving via a path */
    sayArrivingViaPath(traveler, passage)
    {
        "\^<<traveler.travelerName(true)>> enter<<traveler.verbEndingSEd>>
        <<traveler.travelerRemoteLocName>> via <<passage.theNameObj>>. ";
    }

    /* a traveler is leaving up a stairway */
    sayDepartingUpStairs(traveler, stairs)
    {
        "\^<<traveler.travelerName(nil)>> <<traveler.verbToGo>>
        up <<stairs.theNameObj>>. ";
    }

    /* a traveler is leaving down a stairway */
    sayDepartingDownStairs(traveler, stairs)
    {
        "\^<<traveler.travelerName(nil)>> <<traveler.verbToGo>>
        down <<stairs.theNameObj>>. ";
    }

    /* a traveler is arriving by coming up a stairway */
    sayArrivingUpStairs(traveler, stairs)
    {
        local nm = traveler.travelerRemoteLocName;

        "\^<<traveler.travelerName(true)>> <<traveler.verbToCome>>
        up <<stairs.theNameObj>><<nm != '' ? ' to ' + nm : ''>>. ";
    }

    /* a traveler is arriving by coming down a stairway */
    sayArrivingDownStairs(traveler, stairs)
    {
        local nm = traveler.travelerRemoteLocName;

        "\^<<traveler.travelerName(true)>> <<traveler.verbToCome>>
        down <<stairs.theNameObj>><<nm != '' ? ' to ' + nm : ''>>. ";
    }

    /* acompanying another actor on travel */
    sayDepartingWith(traveler, lead)
    {
        "\^<<traveler.travelerName(nil)>> <<traveler.verbToCome>>
        with <<lead.theNameObj>>. ";
    }

    /*
     *   Accompanying a tour guide.  Note the seemingly reversed roles:
     *   the lead actor is the one initiating the travel, and the tour
     *   guide is the accompanying actor.  So, the lead actor is
     *   effectively following the accompanying actor.  It seems
     *   backwards, but really it's not: the tour guide merely shows the
     *   lead actor where to go, but it's up to the lead actor to actually
     *   initiate the travel.  
     */
    sayDepartingWithGuide(guide, lead)
    {
        "\^<<lead.theName>> let<<tSel(lead.verbEndingS, '')>>
        <<guide.theNameObj>> lead the way. ";
    }

    /* note that a door is being opened/closed remotely */
    sayOpenDoorRemotely(door, stat)
    {
        "Someone <<stat ? 'open' + tSel('s', 'ed')
                        : 'close' + tSel('s', 'd')>>
        <<door.theNameObj>> from the other side. ";
    }

    /*
     *   open/closed status - these are simply adjectives that can be used
     *   to describe the status of an openable object 
     */
    openMsg(obj) { return 'open'; }
    closedMsg(obj) { return 'closed'; }

    /* object is currently open/closed */
    currentlyOpen = '{It\'s dobj} currently open. '
    currentlyClosed = '{It\'s dobj} currently closed. '

    /* stand-alone independent clause describing current open status */
    openStatusMsg(obj) { return obj.itIsContraction + ' ' + obj.openDesc; }

    /* locked/unlocked status - adjectives describing lock states */
    lockedMsg(obj) { return 'locked'; }
    unlockedMsg(obj) { return 'unlocked'; }

    /* object is currently locked/unlocked */
    currentlyLocked = '{It\'s dobj} currently locked. '
    currentlyUnlocked = '{It\'s dobj} currently unlocked. '

    /*
     *   on/off status - these are simply adjectives that can be used to
     *   describe the status of a switchable object 
     */
    onMsg(obj) { return 'on'; }
    offMsg(obj) { return 'off'; }

    /* daemon report for burning out a match */
    matchBurnedOut(obj)
    {
        gMessageParams(obj);
        "{The obj/he} finish{es/ed} burning, and disappear{s/ed} into a
        cloud of ash. ";
    }

    /* daemon report for burning out a candle */
    candleBurnedOut(obj)
    {
        gMessageParams(obj);
        "{The obj/he} burn{s/ed} down too far to stay lit, and {goes}
        out. ";
    }

    /* daemon report for burning out a generic fueled light source */
    objBurnedOut(obj)
    {
        gMessageParams(obj);
        "{The obj/he} {goes} out. ";
    }

    /* 
     *   Standard dialog titles, for the Web UI.  These are shown in the
     *   title bar area of the Web UI dialog used for inputDialog() calls.
     *   These correspond to the InDlgIconXxx icons.  The conventional
     *   interpreters use built-in titles when titles are needed at all,
     *   but in the Web UI we have to generate these ourselves. 
     */
    dlgTitleNone = 'Note'
    dlgTitleWarning = 'Warning'
    dlgTitleInfo = 'Note'
    dlgTitleQuestion = 'Question'
    dlgTitleError = 'Error'

    /*
     *   Standard dialog button labels, for the Web UI.  These are built in
     *   to the conventional interpreters, but in the Web UI we have to
     *   generate these ourselves.  
     */
    dlgButtonOk = 'OK'
    dlgButtonCancel = 'Cancel'
    dlgButtonYes = 'Yes'
    dlgButtonNo = 'No'

    /* web UI alert when a new user has joined a multi-user session */
    webNewUser(name) { "\b[<<name>> has joined the session.]\n"; }

    /*
     *   Warning prompt for inputFile() warnings generated when reading a
     *   script file, for the Web UI.  The interpreter normally displays
     *   these warnings directly, but in Web UI mode, the program is
     *   responsible, so we need localized messages.  
     */
    inputFileScriptWarning(warning, filename)
    {
        /* remove the two-letter error code at the start of the string */
        warning = warning.substr(3);

        /* build the message */
        return warning + ' Do you wish to proceed?';
    }
    inputFileScriptWarningButtons = [
        '&Yes, use this file', '&Choose another file', '&Stop the script']
;

/* ------------------------------------------------------------------------ */
/*
 *   Player Character messages.  These messages are generated when the
 *   player issues a regular command to the player character (i.e.,
 *   without specifying a target actor).  
 */
playerMessages: libMessages
    /* invalid command syntax */
    commandNotUnderstood(actor)
    {
        "<.parser>The story doesn&rsquo;t understand that
        command.<./parser> ";
    }

    /* a special topic can't be used right now, because it's inactive */
    specialTopicInactive(actor)
    {
        "<.parser>That command can&rsquo;t be used right now.<./parser> ";
    }

    /* no match for a noun phrase */
    noMatch(actor, action, txt) { action.noMatch(self, actor, txt); }

    /*
     *   No match message - we can't see a match for the noun phrase.  This
     *   is the default for most verbs. 
     */
    noMatchCannotSee(actor, txt)
        { "{You/he} {sees} no <<txt>> {|t}here. "; }

    /*
     *   No match message - we're not aware of a match for the noun phrase.
     *   Some sensory actions, such as LISTEN TO and SMELL, use this
     *   variation instead of the normal version; the things these commands
     *   refer to tend to be intangible, so "you can't see that" tends to
     *   be nonsensical. 
     */
    noMatchNotAware(actor, txt)
        { "{You/he} {are} not aware of any <<txt>> {|t}here. "; }

    /* 'all' is not allowed with the attempted action */
    allNotAllowed(actor)
    {
        "<.parser><q>All</q> cannot be used with that verb.<./parser> ";
    }

    /* no match for 'all' */
    noMatchForAll(actor)
    {
        "<.parser>{You/he} {sees} nothing suitable {|t}here.<./parser> ";
    }

    /* nothing left for 'all' after removing 'except' items */
    noMatchForAllBut(actor)
    {
        "<.parser>{You/he} {sees} nothing else
        {|t}here.<./parser> ";
    }

    /* nothing left in a plural phrase after removing 'except' items */
    noMatchForListBut(actor) { noMatchForAllBut(actor); }

    /* no match for a pronoun */
    noMatchForPronoun(actor, typ, pronounWord)
    {
        /* show the message */
        "<.parser>The word <q><<pronounWord>></q> doesn&rsquo;t refer to
        anything right now.<./parser> ";
    }

    /*
     *   Ask for a missing object - this is called when a command is
     *   completely missing a noun phrase for one of its objects.  
     */
    askMissingObject(actor, action, which)
    {
        reportQuestion('<.parser>\^' + action.whatObj(which)
                       + ' do you want '
                       + (actor.referralPerson == ThirdPerson
                          ? actor.theName : '')
                       + ' to '
                       + action.getQuestionInf(which) + '?<./parser> ');
    }

    /*
     *   An object was missing - this is called under essentially the same
     *   circumstances as askMissingObject, but in cases where interactive
     *   resolution is impossible and we simply wish to report the problem
     *   and do not wish to ask for help.
     */
    missingObject(actor, action, which)
    {
        "<.parser>You need to be more specific
        about <<action.whatObj(which)>> you want
        <<actor.referralPerson == ThirdPerson ? actor.theName : ''>>
        to <<action.getQuestionInf(which)>>.<./parser> ";
    }

    /*
     *   Ask for a missing literal phrase. 
     */
    askMissingLiteral(actor, action, which)
    {
        /* use the standard missing-object message */
        askMissingObject(actor, action, which);
    }

    /*
     *   Show the message for a missing literal phrase.
     */
    missingLiteral(actor, action, which)
    {
        "<.parser>Please be more specific
        about <<action.whatObj(which)>> to
        <<action.getQuestionInf(which)>>.  Try, for example,
        <<action.getQuestionInf(which)>> <q>something</q>.<./parser> ";
    }

    /* reflexive pronoun not allowed */
    reflexiveNotAllowed(actor, typ, pronounWord)
    {
        "<.parser>The story doesn&rsquo;t understand how to use the word
        <q><<pronounWord>></q> like that.<./parser> ";
    }

    /*
     *   a reflexive pronoun disagrees in gender, number, or something
     *   else with its referent 
     */
    wrongReflexive(actor, typ, pronounWord)
    {
        "<.parser>The story doesn&rsquo;t understand what the
        word <q><<pronounWord>></q> refers to.<./parser> ";
    }

    /* no match for a possessive phrase */
    noMatchForPossessive(actor, owner, txt)
    {
        "<.parser>\^<<owner.nameDoes>>
        not appear to have any such thing.<./parser> ";
    }

    /* no match for a plural possessive phrase */
    noMatchForPluralPossessive(actor, txt)
    {
        "<.parser>\^They <<tSel('do', 'did')>> not appear to have any such
        thing.<./parser> ";
    }

    /* no match for a containment phrase */
    noMatchForLocation(actor, loc, txt)
    {
        "<.parser>\^<<actor.nameSees>>
        no <<loc.childInName(txt)>>.<./parser> ";
    }

    /* nothing in a container whose contents are specifically requested */
    nothingInLocation(actor, loc)
    {
        "<.parser>\^<<actor.nameSees>>
        <<loc.childInName('nothing unusual')>>.<./parser> ";
    }

    /* no match for the response to a disambiguation question */
    noMatchDisambig(actor, origPhrase, disambigResponse)
    {
        /*
         *   show the message, leaving the <.parser> tag mode open - we
         *   always show another disambiguation prompt after this message,
         *   so we'll let the prompt close the <.parser> mode 
         */
        "<.parser>That was not one of the choices. ";
    }

    /* empty noun phrase ('take the') */
    emptyNounPhrase(actor)
    {
        "<.parser>You seem to have left out some words.<./parser> ";
    }

    /* 'take zero books' */
    zeroQuantity(actor, txt)
    {
        "<.parser>\^<<actor.theName>> <<actor.verbCant>> do that to zero of
        something.<./parser> ";
    }

    /* insufficient quantity to meet a command request ('take five books') */
    insufficientQuantity(actor, txt, matchList, requiredNum)
    {
        "<.parser>\^<<actor.nameDoes>>n&rsquo;t see that many
        <<txt>> <<tSel('here', 'there')>>.<./parser> ";
    }

    /* a unique object is required, but multiple objects were specified */
    uniqueObjectRequired(actor, txt, matchList)
    {
        "<.parser>You can&rsquo;t use multiple objects there.<./parser> ";
    }

    /* a single noun phrase is required, but a noun list was used */
    singleObjectRequired(actor, txt)
    {
        "<.parser>Multiple objects aren&rsquo;t allowed with that
        command.<./parser> ";
    }

    /*
     *   The answer to a disambiguation question specifies an invalid
     *   ordinal ("the fourth one" when only three choices were offered).
     *   
     *   'ordinalWord' is the ordinal word entered ('fourth' or the like).
     *   'originalText' is the text of the noun phrase that caused the
     *   disambiguation question to be asked in the first place.  
     */
    disambigOrdinalOutOfRange(actor, ordinalWord, originalText)
    {
        /* leave the <.parser> tag open, for the re-prompt that will follow */
        "<.parser>There were not that many choices. ";
    }

    /*
     *   Ask the canonical disambiguation question: "Which x do you
     *   mean...?".  'matchList' is the list of ambiguous objects with any
     *   redundant equivalents removed; and 'fullMatchList' is the full
     *   list, including redundant equivalents that were removed from
     *   'matchList'.
     *   
     *   If askingAgain is true, it means that we're asking the question
     *   again because we got an invalid response to the previous attempt
     *   at the same prompt.  We will have explained the problem, and now
     *   we're going to give the user another crack at the same response.
     *   
     *   To prevent interactive disambiguation, do this:
     *   
     *   throw new ParseFailureException(&ambiguousNounPhrase,
     *.  originalText, matchList, fullMatchList); 
     */
    askDisambig(actor, originalText, matchList, fullMatchList,
                requiredNum, askingAgain, dist)
    {
        /* mark this as a question report with a dummy report */
        reportQuestion('');
        
        /*
         *   Open the "<.parser>" tag, if we're not "asking again."  If we
         *   are asking again, we will already have shown a message
         *   explaining why we're asking again, and that message will have
         *   left us in <.parser> tag mode, so we don't need to open the
         *   tag again. 
         */
        if (!askingAgain)
            "<.parser>";
        
        /*
         *   the question varies depending on whether we want just one
         *   object or several objects in the final result 
         */
        if (requiredNum == 1)
        {
            /*
             *   One object needed - use the original text in the query.
             *   
             *   Note that if we're "asking again," we will have shown an
             *   additional message first explaining *why* we're asking
             *   again, and that message will have left us in <.parser>
             *   tag mode; so we need to close the <.parser> tag in this
             *   case, but we don't need to show a new one. 
             */
            if (askingAgain)
                "Which did you mean,
                <<askDisambigList(matchList, fullMatchList, nil, dist)>>?";
            else
                "Which <<originalText>> do you mean,
                <<askDisambigList(matchList, fullMatchList, nil, dist)>>?";
        }
        else
        {
            /*
             *   Multiple objects required - ask by number, since we can't
             *   easily guess what the plural might be given the original
             *   text.
             *   
             *   As above, we only need to *close* the <.parser> tag if
             *   we're asking again, because we will already have shown a
             *   prompt that opened the tag in this case.  
             */
            if (askingAgain)
                "Which <<spellInt(requiredNum)>> (of
                <<askDisambigList(matchList, fullMatchList, true, dist)>>)
                did you mean?";
            else
                "Which <<spellInt(requiredNum)>>
                (of <<askDisambigList(matchList, fullMatchList,
                                      true, dist)>>) do you mean?";
        }

        /* close the <.parser> tag */
        "<./parser> ";
    }

    /*
     *   we found an ambiguous noun phrase, but we were unable to perform
     *   interactive disambiguation 
     */
    ambiguousNounPhrase(actor, originalText, matchList, fullMatchList)
    {
        "<.parser>The story doesn&rsquo;t know which
        <<originalText>> you mean.<./parser> ";
    }

    /* the actor is missing in a command */
    missingActor(actor)
    {
        "<.parser>You must be more specific about <<
        whomPronoun>> you want<<tSel('', 'ed')>> to address.<./parser> ";
    }

    /* only a single actor can be addressed at a time */
    singleActorRequired(actor)
    {
        "<.parser>You can only address one person at a time.<./parser> ";
    }

    /* cannot change actor mid-command */
    cannotChangeActor()
    {
        "<.parser>You cannot address more than one character on
        a single command line in this story.<./parser> ";
    }

    /*
     *   tell the user they entered a word we don't know, offering the
     *   chance to correct it with "oops" 
     */
    askUnknownWord(actor, txt)
    {
        /* start the message */
        "<.parser>The word <q><<txt>></q> is not necessary in this
        story.<./parser> ";

        /* mention the OOPS command, if appropriate */
        oopsNote();
    }

    /*
     *   tell the user they entered a word we don't know, but don't offer
     *   an interactive way to fix it (i.e., we can't use OOPS at this
     *   point) 
     */
    wordIsUnknown(actor, txt)
    {
        "<.parser>The story doesn&rsquo;t understand that
        command.<./parser> ";
    }

    /* the actor refuses the command because it's busy with something else */
    refuseCommandBusy(targetActor, issuingActor)
    {
        "\^<<targetActor.nameIs>> busy. ";
    }

    /* cannot speak to multiple actors */
    cannotAddressMultiple(actor)
    {
        "<.parser>\^<<actor.theName>> <<actor.verbCannot>> address multiple
        people at once.<./parser> ";
    }

    /* 
     *   Remaining actions on the command line were aborted due to the
     *   failure of the current action.  This is just a hook for the game's
     *   use, if it wants to provide an explanation; by default, we do
     *   nothing.  Note that games that override this will probably want to
     *   use a flag property so that they only show this message once -
     *   it's really only desirable to explain the the mechanism, not to
     *   flag it every time it's used.  
     */
    explainCancelCommandLine()
    {
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Non-Player Character (NPC) messages - parser-mediated format.  These
 *   versions of the NPC messages report errors through the
 *   parser/narrator.
 *   
 *   Note that a separate set of messages can be selected to report
 *   messages in the voice of the NPC - see npcMessagesDirect below.  
 */

/*
 *   Standard Non-Player Character (NPC) messages.  These messages are
 *   generated when the player issues a command to a specific non-player
 *   character. 
 */
npcMessages: playerMessages
    /* the target cannot hear a command we gave */
    commandNotHeard(actor)
    {
        "\^<<actor.nameDoes>> not respond. ";
    }

    /* no match for a noun phrase */
    noMatchCannotSee(actor, txt)
        { "\^<<actor.nameSees>> no <<txt>>. "; }
    noMatchNotAware(actor, txt)
        { "\^<<actor.nameIs>> not aware of any <<txt>>. "; }

    /* no match for 'all' */
    noMatchForAll(actor)
    {
        "<.parser>\^<<actor.nameDoes>>n&rsquo;t see anything
        suitable.<./parser> ";
    }

    /* nothing left for 'all' after removing 'except' items */
    noMatchForAllBut(actor)
    {
        "<.parser>\^<<actor.nameSees>> nothing
        else.<./parser> ";
    }

    /* insufficient quantity to meet a command request ('take five books') */
    insufficientQuantity(actor, txt, matchList, requiredNum)
    {
        "<.parser>\^<<actor.nameDoes>>n&rsquo;t see that many
         <<txt>>.<./parser> ";
    }

    /*
     *   we found an ambiguous noun phrase, but we were unable to perform
     *   interactive disambiguation 
     */
    ambiguousNounPhrase(actor, originalText, matchList, fullMatchList)
    {
        "<.parser>\^<<actor.nameDoes>>n&rsquo;t know which
        <<originalText>> you mean<<tSel('', 't')>>.<./parser> ";
    }

    /*
     *   Missing object query and error message templates 
     */
    askMissingObject(actor, action, which)
    {
        reportQuestion('<.parser>\^' + action.whatObj(which)
                       + ' do you want ' + actor.theNameObj + ' to '
                       + action.getQuestionInf(which) + '?<./parser> ');
    }
    missingObject(actor, action, which)
    {
        "<.parser>You must be more specific
        about <<action.whatObj(which)>> you want <<actor.theNameObj>> to
        <<action.getQuestionInf(which)>>.<./parser> ";
    }

    /* missing literal phrase query and error message templates */
    missingLiteral(actor, action, which)
    {
        "<.parser>You must be more specific about <<action.whatObj(which)>>
        you want <<actor.theNameObj>> to <<action.getQuestionInf(which)>>.
        For example: <<actor.theName>>, <<action.getQuestionInf(which)>>
        <q>something</q>.<./parser> ";
    }
;

/*
 *   Deferred NPC messages.  We use this to report deferred messages from
 *   an NPC to the player.  A message is deferred when a parsing error
 *   occurs, but the NPC can't talk to the player because there's no sense
 *   path to the player.  When this happens, the NPC queues the message
 *   for eventual delivery; when a sense path appears later that lets the
 *   NPC talk to the player, we deliver the message through this object.
 *   Since these messages describe conditions that occurred in the past,
 *   we use the past tense to phrase the messages.
 *   
 *   This default implementation simply doesn't report deferred errors at
 *   all.  The default message voice is the parser/narrator character, and
 *   there is simply no good way for the parser/narrator to say that a
 *   command failed in the past for a given character: "Bob looks like he
 *   didn't know which box you meant" just doesn't work.  So, we'll simply
 *   not report these errors at all.
 *   
 *   To report messages in the NPC's voice directly, modify the NPC's
 *   Actor object, or the Actor base class, to return
 *   npcDeferredMessagesDirect rather than this object from
 *   getParserMessageObj().  
 */
npcDeferredMessages: object
;

/* ------------------------------------------------------------------------ */
/*
 *   NPC messages, reported directly in the voice of the NPC.  These
 *   messages are not selected by default, but a game can use them instead
 *   of the parser-mediated versions by modifying the actor object's
 *   getParserMessageObj() to return these objects.  
 */

/*
 *   Standard Non-Player Character (NPC) messages.  These messages are
 *   generated when the player issues a command to a specific non-player
 *   character. 
 */
npcMessagesDirect: npcMessages
    /* no match for a noun phrase */
    noMatchCannotSee(actor, txt)
    {
        "\^<<actor.nameVerb('look')>> around. <q>I don&rsquo;t
        see any <<txt>>.</q> ";
    }
    noMatchNotAware(actor, txt)
    {
        "<q>I&rsquo;m not aware of any <<txt>>,</q> <<actor.nameSays>>. ";
    }

    /* no match for 'all' */
    noMatchForAll(actor)
    {
        "\^<<actor.nameSays>>, <q>I don&rsquo;t see anything suitable.</q> ";
    }

    /* nothing left for 'all' after removing 'except' items */
    noMatchForAllBut(actor)
    {
        "\^<<actor.nameSays>>, <q>I see nothing else here.</q> ";
    }

    /* 'take zero books' */
    zeroQuantity(actor, txt)
    {
        "\^<<actor.nameSays>>,
        <q>I can&rsquo;t do that to zero of something.</q> ";
    }

    /* insufficient quantity to meet a command request ('take five books') */
    insufficientQuantity(actor, txt, matchList, requiredNum)
    {
        "\^<<actor.nameSays>>,
        <q>I don&rsquo;t see that many <<txt>> here.</q> ";
    }

    /* a unique object is required, but multiple objects were specified */
    uniqueObjectRequired(actor, txt, matchList)
    {
        "\^<<actor.nameSays>>,
        <q>I can&rsquo;t use multiple objects like that.</q> ";
    }

    /* a single noun phrase is required, but a noun list was used */
    singleObjectRequired(actor, txt)
    {
        "\^<<actor.nameSays>>,
        <q>I can&rsquo;t use multiple objects like that.</q> ";
    }

    /* no match for the response to a disambiguation question */
    noMatchDisambig(actor, origPhrase, disambigResponse)
    {
        /* leave the quote open for the re-prompt */
        "\^<<actor.nameSays>>,
        <q>That was not one of the choices. ";
    }

    /*
     *   The answer to a disambiguation question specifies an invalid
     *   ordinal ("the fourth one" when only three choices were offered).
     *   
     *   'ordinalWord' is the ordinal word entered ('fourth' or the like).
     *   'originalText' is the text of the noun phrase that caused the
     *   disambiguation question to be asked in the first place.  
     */
    disambigOrdinalOutOfRange(actor, ordinalWord, originalText)
    {
        /* leave the quote open for the re-prompt */
        "\^<<actor.nameSays>>,
        <q>There weren&rsquo;t that many choices. ";
    }

    /*
     *   Ask the canonical disambiguation question: "Which x do you
     *   mean...?".  'matchList' is the list of ambiguous objects with any
     *   redundant equivalents removed, and 'fullMatchList' is the full
     *   list, including redundant equivalents that were removed from
     *   'matchList'.  
     *   
     *   To prevent interactive disambiguation, do this:
     *   
     *   throw new ParseFailureException(&ambiguousNounPhrase,
     *.  originalText, matchList, fullMatchList); 
     */
    askDisambig(actor, originalText, matchList, fullMatchList,
                requiredNum, askingAgain, dist)
    {
        /* mark this as a question report */
        reportQuestion('');

        /* the question depends on the number needed */
        if (requiredNum == 1)
        {
            /* one required - ask with the original text */
            if (!askingAgain)
                "\^<<actor.nameVerb('ask')>>, <q>";
            
            "Which <<originalText>> do you mean, <<
            askDisambigList(matchList, fullMatchList, nil, dist)>>?</q> ";
        }
        else
        {
            /*
             *   more than one required - we can't guess at the plural
             *   given the original text, so just use the number 
             */
            if (!askingAgain)
                "\^<<actor.nameVerb('ask')>>, <q>";
            
            "Which <<spellInt(requiredNum)>> (of <<
            askDisambigList(matchList, fullMatchList, true, dist)>>)
            do you mean?</q> ";
        }
    }

    /*
     *   we found an ambiguous noun phrase, but we were unable to perform
     *   interactive disambiguation 
     */
    ambiguousNounPhrase(actor, originalText, matchList, fullMatchList)
    {
        "\^<<actor.nameSays>>,
        <q>I don&rsquo;t know which <<originalText>> you mean.</q> ";
    }

    /*
     *   Missing object query and error message templates 
     */
    askMissingObject(actor, action, which)
    {
        reportQuestion('\^' + actor.nameSays
                       + ', <q>\^' + action.whatObj(which)
                       + ' do you want me to '
                       + action.getQuestionInf(which) + '?</q> ');
    }
    missingObject(actor, action, which)
    {
        "\^<<actor.nameSays>>,
        <q>I don&rsquo;t know <<action.whatObj(which)>>
        you want me to <<action.getQuestionInf(which)>>.</q> ";
    }
    missingLiteral(actor, action, which)
    {
        /* use the same message we use for a missing ordinary object */
        missingObject(actor, action, which);
    }

    /* tell the user they entered a word we don't know */
    askUnknownWord(actor, txt)
    {
        "\^<<actor.nameSays>>,
        <q>I don&rsquo;t know the word <q><<txt>></q>.</q> ";
    }

    /* tell the user they entered a word we don't know */
    wordIsUnknown(actor, txt)
    {
        "\^<<actor.nameSays>>,
        <q>You used a word I don&rsquo;t know.</q> ";
    }
;

/*
 *   Deferred NPC messages.  We use this to report deferred messages from
 *   an NPC to the player.  A message is deferred when a parsing error
 *   occurs, but the NPC can't talk to the player because there's no sense
 *   path to the player.  When this happens, the NPC queues the message
 *   for eventual delivery; when a sense path appears later that lets the
 *   NPC talk to the player, we deliver the message through this object.
 *   Since these messages describe conditions that occurred in the past,
 *   we use the past tense to phrase the messages.
 *   
 *   Some messages will never be deferred:
 *   
 *   commandNotHeard - if a command is not heard, it will never enter an
 *   actor's command queue; the error is given immediately in response to
 *   the command entry.
 *   
 *   refuseCommandBusy - same as commandNotHeard
 *   
 *   noMatchDisambig - interactive disambiguation will not happen in a
 *   deferred response situation, so it is impossible to have an
 *   interactive disambiguation failure.  
 *   
 *   disambigOrdinalOutOfRange - for the same reason noMatchDisambig can't
 *   be deferred.
 *   
 *   askDisambig - if we couldn't display a message, we definitely
 *   couldn't perform interactive disambiguation.
 *   
 *   askMissingObject - for the same reason that askDisambig can't be
 *   deferred
 *   
 *   askUnknownWord - for the same reason that askDisambig can't be
 *   deferred.  
 */
npcDeferredMessagesDirect: npcDeferredMessages
    commandNotUnderstood(actor)
    {
        "\^<<actor.nameSays>>,
        <q>I didn&rsquo;t understand what you meant.</q> ";
    }

    /* no match for a noun phrase */
    noMatchCannotSee(actor, txt)
    {
        "\^<<actor.nameSays>>, <q>I didn&rsquo;t see any <<txt>>.</q> ";
    }
    noMatchNotAware(actor, txt)
    {
        "\^<<actor.nameSays>>, <q>I wasn&rsquo;t aware of any <<txt>>.</q> ";
    }

    /* no match for 'all' */
    noMatchForAll(actor)
    {
        "\^<<actor.nameSays>>, <q>I didn&rsquo;t see anything suitable.</q> ";
    }

    /* nothing left for 'all' after removing 'except' items */
    noMatchForAllBut(actor)
    {
        "\^<<actor.nameSays>>,
        <q>I didn&rsquo;t see what you meant.</q> ";
    }

    /* empty noun phrase ('take the') */
    emptyNounPhrase(actor)
    {
        "\^<<actor.nameSays>>,
        <q>You left some words out.</q> ";
    }

    /* 'take zero books' */
    zeroQuantity(actor, txt)
    {
        "\^<<actor.nameSays>>,
        <q>I didn&rsquo;t understand what you meant.</q> ";
    }

    /* insufficient quantity to meet a command request ('take five books') */
    insufficientQuantity(actor, txt, matchList, requiredNum)
    {
        "\^<<actor.nameSays>>,
        <q>I didn&rsquo;t see enough <<txt>>.</q> ";
    }

    /* a unique object is required, but multiple objects were specified */
    uniqueObjectRequired(actor, txt, matchList)
    {
        "\^<<actor.nameSays>>,
        <q>I didn&rsquo;t understand what you meant.</q> ";
    }

    /* a unique object is required, but multiple objects were specified */
    singleObjectRequired(actor, txt)
    {
        "\^<<actor.nameSays>>,
        <q>I didn&rsquo;t understand what you meant.</q> ";
    }

    /*
     *   we found an ambiguous noun phrase, but we were unable to perform
     *   interactive disambiguation 
     */
    ambiguousNounPhrase(actor, originalText, matchList, fullMatchList)
    {
        "\^<<actor.nameSays>>,
        <q>I couldn&rsquo;t tell which <<originalText>> you meant.</q> ";
    }

    /* an object phrase was missing */
    askMissingObject(actor, action, which)
    {
        reportQuestion('\^' + actor.nameSays
                       + ', <q>I didn&rsquo;t know '
                       + action.whatObj(which) + ' you wanted me to '
                       + action.getQuestionInf(which) + '.</q> ');
    }

    /* tell the user they entered a word we don't know */
    wordIsUnknown(actor, txt)
    {
        "\^<<actor.nameSays>>,
        <q>You used a word I don&rsquo;t know.</q> ";
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Verb messages for standard library verb implementations for actions
 *   performed by the player character.  These return strings suitable for
 *   use in VerifyResult objects as well as for action reports
 *   (defaultReport, mainReport, and so on).
 *   
 *   Most of these messages are generic enough to be used for player and
 *   non-player character alike.  However, some of the messages either are
 *   too terse (such as the default reports) or are phrased awkwardly for
 *   NPC use, so the NPC verb messages override those.  
 */
playerActionMessages: MessageHelper
    /*
     *   generic "can't do that" message - this is used when verification
     *   fails because an object doesn't define the action ("doXxx")
     *   method for the verb 
     */
    cannotDoThatMsg = '{You/he} {can\'t} do that. '

    /* must be holding something before a command */
    mustBeHoldingMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} be holding {the obj/him} to do that. ';
    }

    /* it's too dark to do that */
    tooDarkMsg = 'It{&rsquo;s| was} too dark to do that. '

    /* object must be visible */
    mustBeVisibleMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {cannot} see {that obj/him}. ';
    }

    /* object can be heard but not seen */
    heardButNotSeenMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {can} hear {an obj/him}, but {you/he}
                 {can\'t} see {it obj/him}. ';
    }

    /* object can be smelled but not seen */
    smelledButNotSeenMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {can} smell {an obj/him}, but {you/he}
                {can\'t} see {it obj/him}. ';
    }

    /* cannot hear object */
    cannotHearMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {cannot} hear {that obj/him}. ';
    }

    /* cannot smell object */
    cannotSmellMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {cannot} smell {that obj/him}. ';
    }

    /* cannot taste object */
    cannotTasteMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {cannot} taste {that obj/him}. ';
    }

    /* must remove an article of clothing before a command */
    cannotBeWearingMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} take off {the obj/him}
                before {it actor/he} {can} do that. ';
    }

    /* all contents must be removed from object before doing that */
    mustBeEmptyMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} take everything out of {the obj/him}
                before {it actor/he} {can} do that. ';
    }

    /* object must be opened before doing that */
    mustBeOpenMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} open {the obj/him}
                before {it actor/he} {can} do that. ';
    }

    /* object must be closed before doing that */
    mustBeClosedMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} close {the obj/him}
               before {it actor/he} {can} do that. ';
    }

    /* object must be unlocked before doing that */
    mustBeUnlockedMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} unlock {the obj/him}
                before {it actor/he} {can} do that. ';
    }

    /* no key is needed to lock or unlock this object */
    noKeyNeededMsg = '{The dobj/he} {does} not appear to take a key. '

    /* actor must be standing before doing that */
    mustBeStandingMsg = '{You/he} {must} stand up before {it actor/he}
                      {can} do that. '

    /* must be sitting on/in chair */
    mustSitOnMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} sit {in obj} first. ';
    }

    /* must be lying on/in object */
    mustLieOnMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} lie {in obj} first. ';
    }

    /* must get on/in object */
    mustGetOnMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} get {in obj} first. ';
    }

    /* object must be in loc before doing that */
    mustBeInMsg(obj, loc)
    {
        gMessageParams(obj, loc);
        return '{The obj/he} {must} be {in loc} before {you/he}
            {can} do that. ';
    }

    /* actor must be holding the object before we can do that */
    mustBeCarryingMsg(obj, actor)
    {
        gMessageParams(obj, actor);
        return '{The actor/he} {must} be holding {the obj/him}
            before {it actor/he} {can} do that. ';
    }

    /* generic "that's not important" message for decorations */
    decorationNotImportantMsg(obj)
    {
        gMessageParams(obj);
        return '{The obj/he} {is}n&rsquo;t important. ';
    }

    /* generic "you don't see that" message for "unthings" */
    unthingNotHereMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {does}n&rsquo;t see {that obj/him} {|t}here. ';
    }

    /* generic "that's too far away" message for Distant items */
    tooDistantMsg(obj)
    {
        gMessageParams(obj);
        return '{The obj/he} {is} too far away. ';
    }

    /* generic "no can do" message for intangibles */
    notWithIntangibleMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {can\'t} do that to {an obj/him}. ';
    }

    /* generic failure message for varporous objects */
    notWithVaporousMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {can\'t} do that to {an obj/him}. ';
    }

    /* look in/look under/look through/look behind/search vaporous */
    lookInVaporousMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} just {sees} {the obj/him}. ';
    }

    /*
     *   cannot reach (i.e., touch) an object that is to be manipulated in
     *   a command - this is a generic message used when we cannot
     *   identify the specific reason that the object is in scope but
     *   cannot be touched 
     */
    cannotReachObjectMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {cannot} reach {the obj/him}. ';
    }

    /* cannot reach an object through an obstructor */
    cannotReachThroughMsg(obj, loc)
    {
        gMessageParams(obj, loc);
        return '{You/he} {cannot} reach {the obj/him} through '
               + '{the loc/him}. ';
    }

    /* generic long description of a Thing */
    thingDescMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {sees} nothing unusual about '
               + '{it obj/him}. ';
    }

    /* generic LISTEN TO description of a Thing */
    thingSoundDescMsg(obj)
        { return '{You/he} hear{s/d} nothing out of the ordinary. '; }

    /* generic "smell" description of a Thing */
    thingSmellDescMsg(obj)
        { return '{You/he} smell{s/ed} nothing out of the ordinary. '; }

    /* default description of a non-player character */
    npcDescMsg(npc)
    {
        gMessageParams(npc);
        return '{You/he} {sees} nothing unusual about '
               + '{the npc/him}. ';
    }

    /* generic messages for looking prepositionally */
    nothingInsideMsg =
        'There{&rsquo;s| was} nothing unusual in {the dobj/him}. '
    nothingUnderMsg =
        '{You/he} {sees} nothing unusual under {the dobj/him}. '
    nothingBehindMsg =
        '{You/he} {sees} nothing unusual behind {the dobj/him}. '
    nothingThroughMsg =
        '{You/he} {can} see nothing through {the dobj/him}. '

    /* this is an object we can't look behind/through */
    cannotLookBehindMsg = '{You/he} {can\'t} look behind {the dobj/him}. '
    cannotLookUnderMsg = '{You/he} {can\'t} look under {the dobj/him}. '
    cannotLookThroughMsg = '{You/he} {can\'t} see through {the dobj/him}. '

    /* looking through an open passage */
    nothingThroughPassageMsg = '{You/he} {can\'t} see much through
        {the dobj/him} from {|t}here. '

    /* there's nothing on the other side of a door we just opened */
    nothingBeyondDoorMsg = 'Opening {the dobj/him} reveal{s|ed} nothing
        unusual. '

    /* there's nothing here with a specific odor */
    nothingToSmellMsg =
        '{You/he} smell{s/ed} nothing out of the ordinary. '

    /* there's nothing here with a specific noise */
    nothingToHearMsg = '{You/he} hear{s/d} nothing out of the ordinary. '

    /* a sound appears to be coming from a source */
    noiseSourceMsg(src)
    {
        return '{The dobj/he} seem{s/ed} to be coming from '
            + src.theNameObj + '. ';
    }

    /* an odor appears to be coming from a source */
    odorSourceMsg(src)
    {
        return '{The dobj/he} seem{s/ed} to be coming from '
            + src.theNameObj + '. ';
    }

    /* an item is not wearable */
    notWearableMsg =
        '{That dobj/he} {is}n&rsquo;t something {you/he} {can} wear. '

    /* doffing something that isn't wearable */
    notDoffableMsg =
        '{That dobj/he} {is}n&rsquo;t something {you/he} {can} remove. '

    /* already wearing item */
    alreadyWearingMsg = '{You\'re} already wearing {it dobj/him}. '

    /* not wearing (item being doffed) */
    notWearingMsg = '{You\'re} not wearing {that dobj/him}. '

    /* default response to 'wear obj' */
    okayWearMsg = 'Okay, {you\'re} now wearing {the dobj/him}. '

    /* default response to 'doff obj' */
    okayDoffMsg = 'Okay, {you\'re} no longer wearing {the dobj/him}. '

    /* default response to open/close */
    okayOpenMsg = shortTMsg(
        'Opened. ', '{You/he} open{s/ed} {the dobj/him}. ')
    okayCloseMsg = shortTMsg(
        'Closed. ', '{You/he} close{s/d} {the dobj/him}. ')

    /* default response to lock/unlock */
    okayLockMsg = shortTMsg(
        'Locked. ', '{You/he} lock{s/ed} {the dobj/him}. ')
    okayUnlockMsg = shortTMsg(
        'Unlocked. ', '{You/he} unlock{s/ed} {the dobj/him}. ')

    /* cannot dig here */
    cannotDigMsg = '{You/he} {have} no reason to dig in {that dobj/him}. '

    /* not a digging implement */
    cannotDigWithMsg =
        '{You/he} {sees} no way to use {that iobj/him} as a shovel. '

    /* taking something already being held */
    alreadyHoldingMsg = '{You/he} {are} already carrying {the dobj/him}. '

    /* actor taking self ("take me") */
    takingSelfMsg = '{You/he} {can\'t} take {yourself}. '

    /* dropping an object not being carried */
    notCarryingMsg = '{You\'re} not carrying {that dobj/him}. '

    /* actor dropping self */
    droppingSelfMsg = '{You/he} {can\'t} drop {yourself}. '

    /* actor putting self in something */
    puttingSelfMsg = '{You/he} {can\'t} do that to {yourself}. '

    /* actor throwing self */
    throwingSelfMsg = '{You/he} {can\'t} throw {yourself}. '

    /* we can't put the dobj in the iobj because it's already there */
    alreadyPutInMsg = '{The dobj/he} {is} already in {the iobj/him}. '

    /* we can't put the dobj on the iobj because it's already there */
    alreadyPutOnMsg = '{The dobj/he} {is} already on {the iobj/him}. '

    /* we can't put the dobj under the iobj because it's already there */
    alreadyPutUnderMsg = '{The dobj/he} {is} already under {the iobj/him}. '

    /* we can't put the dobj behind the iobj because it's already there */
    alreadyPutBehindMsg = '{The dobj/he} {is} already behind {the iobj/him}. '

    /*
     *   trying to move a Fixture to a new container by some means (take,
     *   drop, put in, put on, etc) 
     */
    cannotMoveFixtureMsg = '{The dobj/he} {cannot} be moved. '

    /* trying to take a Fixture */
    cannotTakeFixtureMsg = '{You/he} {can\'t} take {that dobj/him}. '

    /* trying to put a Fixture in something */
    cannotPutFixtureMsg = '{You/he} {can\'t} put {the dobj/him} anywhere. '

    /* trying to take/move/put an Immovable object */
    cannotTakeImmovableMsg = '{You/he} {can\'t} take {that dobj/him}. '
    cannotMoveImmovableMsg = '{The dobj/he} {cannot} be moved. '
    cannotPutImmovableMsg = '{You/he} {can\'t} put {the dobj/him} anywhere. '

    /* trying to take/move/put a Heavy object */
    cannotTakeHeavyMsg = '{The dobj/he} {is} too heavy. '
    cannotMoveHeavyMsg = '{The dobj/he} {is} too heavy. '
    cannotPutHeavyMsg = '{The dobj/he} {is} too heavy. '

    /* trying to move a component object */
    cannotMoveComponentMsg(loc)
    {
        return '{The dobj/he} {is} part of ' + loc.theNameObj + '. ';
    }

    /* trying to take a component object */
    cannotTakeComponentMsg(loc)
    {
        return '{You/he} {can\'t} have {that/him dobj}; '
            + '{it\'s dobj} part of ' + loc.theNameObj + '. ';
    }

    /* trying to put a component in something */
    cannotPutComponentMsg(loc)
    {
        return '{You/he} {can\'t} put {that/him dobj} anywhere; '
            + '{it\'s dobj} part of ' + loc.theNameObj + '. ';
    }

    /* specialized Immovable messages for TravelPushables */
    cannotTakePushableMsg = '{You/he} {can\'t} take {that/him dobj}, but
        {it actor/he} might {be|have been} able to push it somewhere. '
    cannotMovePushableMsg = 'It wouldn&rsquo;t {|have} accomplish{|ed}
        anything to move {the dobj/him} around aimlessly, but {it actor/he}
        might {be|have been} able to push {it dobj/him} in a specific
        direction. '
    cannotPutPushableMsg = '{You/he} {can\'t} put {that/him dobj} anywhere,
        but {it actor/he} might {be|have been} able to push it somewhere. '

    /* can't take something while occupying it */
    cannotTakeLocationMsg = '{You/he} {can\'t} take {that/him dobj}
        while {you\'re} occupying {it/him dobj}. '

    /* can't REMOVE something that's being held */
    cannotRemoveHeldMsg = 'There{\'s| was} nothing to remove
        {the dobj/him} from. '

    /* default 'take' response */
    okayTakeMsg = shortTMsg(
        'Taken. ', '{You/he} {take[s]|took} {the dobj/him}. ')

    /* default 'drop' response */
    okayDropMsg = shortTMsg(
        'Dropped. ', '{You/he} drop{s/ped} {the dobj/him}. ')

    /* dropping an object */
    droppingObjMsg(dropobj)
    {
        gMessageParams(dropobj);
        return '{You/he} drop{s/ped} {the dropobj/him}. ';
    }

    /* default receiveDrop suffix for floorless rooms */
    floorlessDropMsg(dropobj)
    {
        gMessageParams(dropobj);
        return '{It dropobj/he} {fall[s]|fell} out of sight below. ';
    }

    /* default successful 'put in' response */
    okayPutInMsg = shortTIMsg(
        'Done. ', '{You/he} put{[s]|} {the dobj/him} in {the iobj/him}. ')

    /* default successful 'put on' response */
    okayPutOnMsg = shortTIMsg(
        'Done. ', '{You/he} put{[s]|} {the dobj/him} on {the iobj/him}. ')

    /* default successful 'put under' response */
    okayPutUnderMsg = shortTIMsg(
        'Done. ', '{You/he} put{[s]|} {the dobj/him} under {the iobj/him}. ')

    /* default successful 'put behind' response */
    okayPutBehindMsg = shortTIMsg(
        'Done. ', '{You/he} put{[s]|} {the dobj/him} behind {the iobj/him}. ')

    /* try to take/move/put/taste an untakeable actor */
    cannotTakeActorMsg = '{The dobj/he} {won\'t} let {you/him} do that. '
    cannotMoveActorMsg = '{The dobj/he} {won\'t} let {you/him} do that. '
    cannotPutActorMsg = '{The dobj/he} {won\'t} let {you/him} do that. '
    cannotTasteActorMsg = '{The dobj/he} {won\'t} let {you/him} do that. '

    /* trying to take/move/put/taste a person */
    cannotTakePersonMsg =
        '{The dobj/he} probably wouldn&rsquo;t {|have} like{|d} that. '
    cannotMovePersonMsg =
        '{The dobj/he} probably wouldn&rsquo;t {|have} like{|d} that. '
    cannotPutPersonMsg =
        '{The dobj/he} probably wouldn&rsquo;t {|have} like{|d} that. '
    cannotTastePersonMsg =
        '{The dobj/he} probably wouldn&rsquo;t {|have} like{|d} that. '

    /* cannot move obj through obstructor */
    cannotMoveThroughMsg(obj, obs)
    {
        gMessageParams(obj, obs);
        return '{You/he} {can\'t} move {that obj/him} through '
               + '{the obs/him}. ';
    }

    /* cannot move obj in our out of container cont */
    cannotMoveThroughContainerMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{You/he} {can\'t} move {that obj/him} through '
               + '{the cont/him}. ';
    }

    /* cannot move obj because cont is closed */
    cannotMoveThroughClosedMsg(obj, cont)
    {
        gMessageParams(cont);
        return '{You/he} {can\'t} do that because {the cont/he} {is} '
               + 'closed. ';
    }

    /* cannot fit obj into cont through cont's opening */
    cannotFitIntoOpeningMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{You/he} {can\'t} do that because {the obj/he} {is}
                too big to put into {the cont/him}. ';
    }

    /* cannot fit obj out of cont through cont's opening */
    cannotFitOutOfOpeningMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{You/he} {can\'t} do that because {the obj/he} {is}
                too big to take out of {the cont/him}. ';
    }

    /* actor 'obj' cannot reach in our out of container 'cont' */
    cannotTouchThroughContainerMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{The obj/he} {can\'t} reach anything through '
               + '{the cont/him}. ';
    }

    /* actor 'obj' cannot reach through cont because cont is closed */
    cannotTouchThroughClosedMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{The obj/he} {can\'t} do that because
               {the cont/he} {is} closed. ';
    }

    /* actor cannot fit hand into cont through cont's opening */
    cannotReachIntoOpeningMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{The obj/he} {can\'t} fit {its/her} hand into '
               + '{the cont/him}. ';
    }

    /* actor cannot fit hand into cont through cont's opening */
    cannotReachOutOfOpeningMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{The obj/he} {can\'t} fit {its/her} hand through
               {the cont/him}. ';
    }

    /* the object is too large for the actor to hold */
    tooLargeForActorMsg(obj)
    {
        gMessageParams(obj);
        return '{The obj/he} {is} too large for {you/him} to hold. ';
    }

    /* the actor doesn't have room to hold the object */
    handsTooFullForMsg(obj)
    {
        return '{Your} hands {are|were} too full to hold '
               + obj.theNameObj + '. ';
    }

    /* the object is becoming too large for the actor to hold */
    becomingTooLargeForActorMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {cannot} do that because {the obj/he}
                would {|have} become too large for {you/him} to hold. ';
    }

    /* the object is becoming large enough that the actor's hands are full */
    handsBecomingTooFullForMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {cannot} do that because {its/her} hands
            would {|have} become too full to hold {the obj/him}. ';
    }

    /* the object is too heavy (all by itself) for the actor to hold */
    tooHeavyForActorMsg(obj)
    {
        gMessageParams(obj);
        return '{The obj/he} {is} too heavy for {you/him} to pick up. ';
    }

    /*
     *   the object is too heavy (in combination with everything else
     *   being carried) for the actor to pick up 
     */
    totalTooHeavyForMsg(obj)
    {
        gMessageParams(obj);
        return '{The obj/he} {is} too heavy; {you/he} {will} have to put
            something else down first. ';
    }

    /* object is too large for container */
    tooLargeForContainerMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{The obj/he} {is} too large for {the cont/him}. ';
    }

    /* object is too large to fit under object */
    tooLargeForUndersideMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{The obj/he} {is} too large to put under {the cont/him}. ';
    }

    /* object is too large to fit behind object */
    tooLargeForRearMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{The obj/he} {is} too large to put behind {the cont/him}. ';
    }

    /* container doesn't have room for object */
    containerTooFullMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{The cont/he} {is} already too full to hold {the obj/him}. ';
    }

    /* surface doesn't have room for object */
    surfaceTooFullMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return 'There{&rsquo;s| was} no room for {the obj/him} on '
               + '{the cont/him}. ';
    }

    /* underside doesn't have room for object */
    undersideTooFullMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return 'There{&rsquo;s| was} no room for {the obj/him} under '
               + '{the cont/him}. ';
    }

    /* rear surface/space doesn't have room for object */
    rearTooFullMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return 'There{&rsquo;s |was} no room for {the obj/him} behind '
               + '{the cont/him}. ';
    }

    /* the current action would make obj too large for its container */
    becomingTooLargeForContainerMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{You/he} {cannot} do that because it would {make|have made}
            {the obj/him} too large for {the cont/him}. ';
    }

    /*
     *   the current action would increase obj's bulk so that container is
     *   too full 
     */
    containerBecomingTooFullMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{You/he} {cannot} do that because {the obj/he}
            would no longer {|have} fit{|ted} in {the cont/him}. ';
    }

    /* trying to put an object in a non-container */
    notAContainerMsg = '{You/he} {can\'t} put anything in {the iobj/him}. '

    /* trying to put an object on a non-surface */
    notASurfaceMsg = 'There{&rsquo;s| was} no good surface on
        {the iobj/him}. '

    /* can't put anything under iobj */
    cannotPutUnderMsg =
        '{You/he} {can\'t} put anything under {that iobj/him}. '

    /* nothing can be put behind the given object */
    cannotPutBehindMsg = '{You/he} {can\'t} put anything
        behind {the iobj/him}. '

    /* trying to put something in itself */
    cannotPutInSelfMsg = '{You/he} {can\'t} put {the dobj/him} in {itself}. '

    /* trying to put something on itself */
    cannotPutOnSelfMsg = '{You/he} {can\'t} put {the dobj/him} on {itself}. '

    /* trying to put something under itself */
    cannotPutUnderSelfMsg = '{You/he} {can\'t} put {the dobj/him} under
        {itself}. '

    /* trying to put something behind itself */
    cannotPutBehindSelfMsg = '{You/he} {can\'t} put {the dobj/him} behind
        {itself}. '

    /* can't put something in/on/etc a restricted container/surface/etc */
    cannotPutInRestrictedMsg =
        '{You/he} {can\'t} put {that dobj/him} in {the iobj/him}. '
    cannotPutOnRestrictedMsg =
        '{You/he} {can\'t} put {that dobj/him} on {the iobj/him}. '
    cannotPutUnderRestrictedMsg =
        '{You/he} {can\'t} put {that dobj/him} under {the iobj/him}. '
    cannotPutBehindRestrictedMsg =
        '{You/he} {can\'t} put {that dobj/him} behind {the iobj/him}. '

    /* trying to return something to a remove-only dispenser */
    cannotReturnToDispenserMsg =
        '{You/he} {can\'t} put {a dobj/him} back in {the iobj/him}. '

    /* wrong item type for dispenser */
    cannotPutInDispenserMsg =
        '{You/he} {can\'t} put {a dobj/him} in {the iobj/him}. '

    /* the dobj doesn't fit on this keyring */
    objNotForKeyringMsg = '{The dobj/he} {does}n&rsquo;t fit
        on {the iobj/him}. '

    /* the dobj isn't on the keyring */
    keyNotOnKeyringMsg = '{The dobj/he} {is} not attached to {the iobj/him}. '

    /* can't detach key (with no iobj specified) because it's not on a ring */
    keyNotDetachableMsg = '{The dobj/he} {is}n&rsquo;t attached to anything. '

    /* we took a key and attached it to a keyring */
    takenAndMovedToKeyringMsg(keyring)
    {
        gMessageParams(keyring);
        return '{You/he} pick{s/ed} up {the dobj/him} and
            attach{es/ed actor} {it dobj/him} to {the keyring/him}. ';
    }

    /* we attached a key to a keyring automatically */
    movedKeyToKeyringMsg(keyring)
    {
        gMessageParams(keyring);
        return '{You/he} attach{es/ed} {the dobj/him} to {the keyring/him}. ';
    }

    /* we moved several keys to a keyring automatically */
    movedKeysToKeyringMsg(keyring, keys)
    {
        gMessageParams(keyring);
        return '{You/he} attach{es/ed} {your/his} loose key'
            + (keys.length() > 1 ? 's' : '')
            + ' to {the keyring/him}. ';
    }

    /* putting y in x when x is already in y */
    circularlyInMsg(x, y)
    {
        gMessageParams(x, y);
        return '{You/he} {can\'t} do that while {the x/he} {is}
            in {the y/him}. ';
    }

    /* putting y in x when x is already on y */
    circularlyOnMsg(x, y)
    {
        gMessageParams(x, y);
        return '{You/he} {can\'t} do that while {the x/he} {is}
            on {the y/him}. ';
    }

    /* putting y in x when x is already under y */
    circularlyUnderMsg(x, y)
    {
        gMessageParams(x, y);
        return '{You/he} {can\'t} do that while {the x/he} {is}
            under {the y/him}. ';
    }

    /* putting y in x when x is already behind y */
    circularlyBehindMsg(x, y)
    {
        gMessageParams(x, y);
        return '{You/he} {can\'t} do that while {the x/he} {is}
            behind {the y/him}. ';
    }

    /* taking dobj from iobj, but dobj isn't in iobj */
    takeFromNotInMsg = '{The dobj/he} {is}n&rsquo;t in {that iobj/him}. '

    /* taking dobj from surface, but dobj isn't on iobj */
    takeFromNotOnMsg = '{The dobj/he} {is}n&rsquo;t on {that iobj/him}. '

    /* taking dobj from under something, but dobj isn't under iobj */
    takeFromNotUnderMsg = '{The dobj/he} {is}n&rsquo;t
        under {that iobj/him}. '

    /* taking dobj from behind something, but dobj isn't behind iobj */
    takeFromNotBehindMsg = '{The dobj/he} {is}n&rsquo;t
        behind {that iobj/him}. '

    /* taking dobj from an actor, but actor doesn't have iobj */
    takeFromNotInActorMsg = '{The iobj/he} {does}n&rsquo;t
        have {that dobj/him}. '

    /* actor won't let go of a possession */
    willNotLetGoMsg(holder, obj)
    {
        gMessageParams(holder, obj);
        return '{The holder/he} {won\'t} let {you/him} have {that obj/him}. ';
    }

    /* must say which way to go */
    whereToGoMsg = 'You&rsquo;ll have to say which way to go. '

    /* travel attempted in a direction with no exit */
    cannotGoThatWayMsg = '{You/he} {can\'t} go that way. '

    /* travel attempted in the dark in a direction with no exit */
    cannotGoThatWayInDarkMsg = 'It{&rsquo;s| was} too dark; {you/he}
        {can\'t} see where {you\'re} going. '

    /* we don't know the way back for a GO BACK */
    cannotGoBackMsg = '{You/he} {does}n&rsquo;t know how to return
        from {|t}here. '

    /* cannot carry out a command from this location */
    cannotDoFromHereMsg = '{You/he} {can\'t} do that from {|t}here. '

    /* can't travel through a close door */
    cannotGoThroughClosedDoorMsg(door)
    {
        gMessageParams(door);
        return '{You/he} {can\'t} do that, because {the door/he} {is} '
               + 'closed. ';
    }

    /* cannot carry out travel while 'dest' is within 'cont' */
    invalidStagingContainerMsg(cont, dest)
    {
        gMessageParams(cont, dest);
        return '{You/he} {can\'t} do that while {the dest/he} {is}
                {in cont}. ';
    }

    /* cannot carry out travel while 'cont' (an actor) is holding 'dest' */
    invalidStagingContainerActorMsg(cont, dest)
    {
        gMessageParams(cont, dest);
        return '{You/he} {can\'t} do that while {the cont/he} {is}
            holding {the dest/him}. ';
    }
    
    /* can't carry out travel because 'dest' isn't a valid staging location */
    invalidStagingLocationMsg(dest)
    {
        gMessageParams(dest);
        return '{You/he} {can\'t} get {in dest}. ';
    }

    /* destination is too high to enter from here */
    nestedRoomTooHighMsg(obj)
    {
        gMessageParams(obj);
        return '{The obj/he} {is} too high to reach from {|t}here. ';
    }

    /* enclosing room is too high to reach by GETTING OUT OF here */
    nestedRoomTooHighToExitMsg(obj)
    {
        return 'It{&rsquo;s| was} too long a drop to do that from {|t}here. ';
    }

    /* cannot carry out a command from a nested room */
    cannotDoFromMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {can\'t} do that from {the obj/him}. ';
    }

    /* cannot carry out a command from within a vehicle in a nested room */
    vehicleCannotDoFromMsg(obj)
    {
        local loc = obj.location;
        gMessageParams(obj, loc);
        return '{You/he} {can\'t} do that while {the obj/he} {is} {in loc}. ';
    }

    /* cannot go that way in a vehicle */
    cannotGoThatWayInVehicleMsg(traveler)
    {
        gMessageParams(traveler);
        return '{You/he} {can\'t} do that {in traveler}. ';
    }

    /* cannot push an object that way */
    cannotPushObjectThatWayMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {can\'t} go that way pushing {the obj/him}. ';
    }

    /* cannot push an object to a nested room */
    cannotPushObjectNestedMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {can\'t} push {the obj/him} there. ';
    }

    /* cannot enter an exit-only passage */
    cannotEnterExitOnlyMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {can\'t} enter {the obj/him} from {|t}here. ';
    }

    /* must open door before going that way */
    mustOpenDoorMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} open {the obj/him} first. ';
    }

    /* door closes behind actor during travel through door */
    doorClosesBehindMsg(obj)
    {
        gMessageParams(obj);
        return '<.p>After {you/he} {goes} through {the obj/him}, {it/he}
                close{s/d} behind {it actor/him}. ';
    }

    /* the stairway does not go up/down */
    stairwayNotUpMsg = '{The dobj/he} only {goes} down from {|t}here. '
    stairwayNotDownMsg = '{The dobj/he} only {goes} up from {|t}here. '

    /* "wait" */
    timePassesMsg = 'Time pass{es|ed}... '

    /* "hello" with no target actor */
    sayHelloMsg = (addressingNoOneMsg)

    /* "goodbye" with no target actor */
    sayGoodbyeMsg = (addressingNoOneMsg)

    /* "yes"/"no" with no target actor */
    sayYesMsg = (addressingNoOneMsg)
    sayNoMsg = (addressingNoOneMsg)

    /* an internal common handler for sayHelloMsg, sayGoodbyeMsg, etc */
    addressingNoOneMsg
    {
        return '{You/he} {must} be more specific about '
            + gLibMessages.whomPronoun
            + ' {it actor/he} want{s/ed} to talk to. ';
    }

    /* "yell" */
    okayYellMsg = '{You/he} scream{s/ed} as loud as {it actor/he} {can}. '

    /* "jump" */
    okayJumpMsg = '{You/he} jump{s/ed} a little, and land{s/ed} back where
        {it actor/he} {|had} started. '

    /* cannot jump over object */
    cannotJumpOverMsg = '{You/he} {can\'t} jump over {that dobj/him}. '

    /* cannot jump off object */
    cannotJumpOffMsg = '{You/he} {can\'t} jump off {that dobj/him}. '

    /* cannot jump off (with no direct object) from here */
    cannotJumpOffHereMsg = 'There{&rsquo;s| was} nowhere to jump
        from {|t}here. '

    /* failed to find a topic in a consultable object */
    cannotFindTopicMsg =
        '{You/he} {can\'t} seem to find that in {the dobj/him}. '

    /* an actor doesn't accept a command from another actor */
    refuseCommand(targetActor, issuingActor)
    {
        gMessageParams(targetActor, issuingActor);
        return '{The targetActor/he} refuse{s/d} {your/his} request. ';
    }

    /* cannot talk to an object (because it makes no sense to do so) */
    notAddressableMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {cannot} talk to {that obj/him}. ';
    }

    /* actor won't respond to a request or other communicative gesture */
    noResponseFromMsg(other)
    {
        gMessageParams(other);
        return '{The other/he} {does} not respond. ';
    }

    /* trying to give something to someone who already has the object */
    giveAlreadyHasMsg = '{The iobj/he} already {has} {that/him dobj}. '

    /* can't talk to yourself */
    cannotTalkToSelfMsg = 'Talking to {yourself/himself}
        {won&rsquo;t|wouldn&rsquo;t} accomplish anything. '

    /* can't ask yourself about anything */
    cannotAskSelfMsg = 'Talking to {yourself/himself}
        {won&rsquo;t|wouldn&rsquo;t} accomplish anything. '

    /* can't ask yourself for anything */
    cannotAskSelfForMsg = 'Talking to {yourself/himself}
        {won&rsquo;t|wouldn&rsquo;t} accomplish anything. '

    /* can't tell yourself about anything */
    cannotTellSelfMsg = 'Talking to {yourself/himself}
        {won&rsquo;t|wouldn&rsquo;t} accomplish anything. '

    /* can't give yourself something */
    cannotGiveToSelfMsg = 'Giving {the dobj/him} to {yourself/himself}
        {won&rsquo;t|wouldn&rsquo;t} accomplish anything. '
    
    /* can't give something to itself */
    cannotGiveToItselfMsg = 'Giving {the dobj/him} to {itself}
        {won&rsquo;t|wouldn&rsquo;t} accomplish anything. '

    /* can't show yourself something */
    cannotShowToSelfMsg = 'Showing {the dobj/him} to {yourself/himself}
        {won&rsquo;t|wouldn&rsquo;t} accomplish anything. '

    /* can't show something to itself */
    cannotShowToItselfMsg = 'Showing {the dobj/him} to {itself}
        {won&rsquo;t|wouldn&rsquo;t} accomplish anything. '

    /* can't give/show something to a non-actor */
    cannotGiveToMsg = '{You/he} {cannot} give anything to {an iobj/him}. '
    cannotShowToMsg = '{You/he} {cannot} show anything to {an iobj/him}. '

    /* actor isn't interested in something being given/shown */
    notInterestedMsg(actor)
    {
        return '\^' + actor.nameDoes + ' not appear interested. ';
    }

    /* vague ASK/TELL (for ASK/TELL <actor> <topic> syntax errors) */
    askVagueMsg = '<.parser>The story doesn&rsquo;t understand that command.
        Please use ASK ACTOR ABOUT TOPIC (or just A TOPIC).<./parser> '
    tellVagueMsg = '<.parser>The story doesn&rsquo;t understand that command.
        Please use TELL ACTOR ABOUT TOPIC (or just T TOPIC).<./parser> '

    /* object cannot hear actor */
    objCannotHearActorMsg(obj)
    {
        return '\^' + obj.nameDoes
            + ' not appear to hear {you/him}. ';
    }

    /* actor cannot see object being shown to actor */
    actorCannotSeeMsg(actor, obj)
    {
        return '\^' + actor.nameDoes + ' not appear to be able to see '
            + obj.theNameObj + '. ';
    }

    /* not a followable object */
    notFollowableMsg = '{You/he} {cannot} follow {that dobj/him}. '

    /* cannot follow yourself */
    cannotFollowSelfMsg = '{You/he} {cannot} follow {yourself}. '

    /* following an object that's in the same location as the actor */
    followAlreadyHereMsg = '{The dobj/he} {is} right {|t}here. '

    /*
     *   following an object that we *think* is in our same location (in
     *   other words, we're already in the location where we thought we
     *   last saw the object go), but it's too dark to see if that's
     *   really true 
     */
    followAlreadyHereInDarkMsg = '{The dobj/he} should {be|have been} right
        {|t}here, but {you/he} {can\'t} see {it dobj/him}. '

    /* trying to follow an object, but don't know where it went from here */
    followUnknownMsg = '{You\'re} not sure where {the dobj/he}
        {went|had gone} from {|t}here. '

    /*
     *   we're trying to follow an actor, but we last saw the actor in the
     *   given other location, so we have to go there to follow 
     */
    cannotFollowFromHereMsg(srcLoc)
    {
        return 'The last place {you/he} {saw|had seen} {the dobj/him} was '
            + srcLoc.getDestName(gActor, gActor.location) + '. ';
    }

    /* acknowledge a 'follow' for a target that was in sight */
    okayFollowInSightMsg(loc)
    {
        return '{You/he} follow{s/ed} {the dobj/him} '
            + loc.actorIntoName + '. ';
    }

    /* obj is not a weapon */
    notAWeaponMsg = '{You/he} {can\'t} attack anything with {the iobj/him}. '

    /* no effect attacking obj */
    uselessToAttackMsg = '{You/he} {cannot} attack {that dobj/him}. '

    /* pushing object has no effect */
    pushNoEffectMsg = 'Pushing {the dobj/him} {has|had} no effect. '

    /* default 'push button' acknowledgment */
    okayPushButtonMsg = '<q>Click.</q> '

    /* lever is already in pushed state */
    alreadyPushedMsg =
        '{It\'s dobj} already pushed as far as {it dobj/he} {will} go. '

    /* default acknowledgment to pushing a lever */
    okayPushLeverMsg = '{You/he} push{es/ed} {the dobj/him} to
                     {its/her dobj} stop. '

    /* pulling object has no effect */
    pullNoEffectMsg = 'Pulling {the dobj/him} {has|had} no effect. '

    /* lever is already in pulled state */
    alreadyPulledMsg =
        '{It\'s dobj} already pulled as far as {it dobj/he} {will} go. '

    /* default acknowledgment to pulling a lever */
    okayPullLeverMsg = '{You/he} pull{s/ed} {the dobj/him} to
                     {its/her dobj} stop. '

    /* default acknowledgment to pulling a spring-loaded lever */
    okayPullSpringLeverMsg = '{You/he} pull{s/ed} {the dobj/him}, which
        spr{ing[s]|ung} back to {its/her} starting position as soon as
        {you/he} let{[s]|} go of {it dobj/him}. '

    /* moving object has no effect */
    moveNoEffectMsg = 'Moving {the dobj/him} {has|had} no effect. '

    /* cannot move object to other object */
    moveToNoEffectMsg = 'This would {|have} accomplish{|ed} nothing. '

    /* cannot push an object through travel */
    cannotPushTravelMsg = 'This would {|have} accomplish{|ed} nothing. '

    /* acknowledge pushing an object through travel */
    okayPushTravelMsg(obj)
    {
        return '<.p>{You/he} push{es/ed} ' + obj.theNameObj
            + ' into the area. ';
    }

    /* cannot use object as an implement to move something */
    cannotMoveWithMsg =
        '{You/he} {cannot} move anything with {the iobj/him}. '

    /* cannot set object to setting */
    cannotSetToMsg = '{You/he} {cannot} set {that dobj/him} to anything. '

    /* invalid setting for generic Settable */
    setToInvalidMsg = '{The dobj/he} {has} no such setting. '

    /* default 'set to' acknowledgment */
    okaySetToMsg(val)
        { return 'Okay, {the dobj/he} {is} now set to ' + val + '. '; }

    /* cannot turn object */
    cannotTurnMsg = '{You/he} {cannot} turn {that dobj/him}. '

    /* must specify setting to turn object to */
    mustSpecifyTurnToMsg = '{You/he} {must} specify the setting to
                         turn {it dobj/him} to. '

    /* cannot turn anything with object */
    cannotTurnWithMsg =
        '{You/he} {cannot} turn anything with {that iobj/him}. '

    /* invalid setting for dial */
    turnToInvalidMsg = '{The dobj/he} {has} no such setting. '

    /* default 'turn to' acknowledgment */
    okayTurnToMsg(val)
        { return 'Okay, {the dobj/he} {is} now set to ' + val + '. '; }

    /* switch is already on/off */
    alreadySwitchedOnMsg = '{The dobj/he} {is} already on. '
    alreadySwitchedOffMsg = '{The dobj/he} {is} already off. '

    /* default acknowledgment for switching on/off */
    okayTurnOnMsg = 'Okay, {the dobj/he} {is} now on. '
    okayTurnOffMsg = 'Okay, {the dobj/he} {is} now off. '

    /* flashlight is on but doesn't light up */
    flashlightOnButDarkMsg = '{You/he} turn{s/ed} on {the dobj/him}, but
        nothing seem{s|ed} to happen. '

    /* default acknowledgment for eating something */
    okayEatMsg = '{You/he} {eat[s]|ate} {the dobj/him}. '

    /* object must be burning before doing that */
    mustBeBurningMsg(obj)
    {
        return '{You/he} {must} light ' + obj.theNameObj
            + ' before {it actor/he} {can} do that. ';
    }

    /* match not lit */
    matchNotLitMsg = '{The dobj/he} {is}n&rsquo;t lit. '

    /* lighting a match */
    okayBurnMatchMsg =
        '{You/he} str{ike[s]|uck} {the dobj/him}, igniting a small flame. '

    /* extinguishing a match */
    okayExtinguishMatchMsg = '{You/he} put{[s]|} out {the dobj/him}, which
        disappear{s/ed} into a cloud of ash. '

    /* trying to light a candle with no fuel */
    candleOutOfFuelMsg =
        '{The dobj/he} {is} too burned down; {it/he} {cannot} be lit. '

    /* lighting a candle */
    okayBurnCandleMsg = '{You/he} {light[s]|lit} {the dobj/him}. '

    /* extinguishing a candle that isn't lit */
    candleNotLitMsg = '{The dobj/he} {is} not lit. '

    /* extinguishing a candle */
    okayExtinguishCandleMsg = 'Done. '

    /* cannot consult object */
    cannotConsultMsg =
        '{That dobj/he} {is} not something {you/he} {can} consult. '

    /* cannot type anything on object */
    cannotTypeOnMsg = '{You/he} {cannot} type anything on {that dobj/him}. '

    /* cannot enter anything on object */
    cannotEnterOnMsg = '{You/he} {cannot} enter anything on {that dobj/him}. '

    /* cannot switch object */
    cannotSwitchMsg = '{You/he} {cannot} switch {that dobj/him}. '

    /* cannot flip object */
    cannotFlipMsg = '{You/he} {cannot} flip {that dobj/him}. '

    /* cannot turn object on/off */
    cannotTurnOnMsg =
        '{That dobj/he} {is}n&rsquo;t something {you/he} {can} turn on. '
    cannotTurnOffMsg =
        '{That dobj/he} {is}n&rsquo;t something {you/he} {can} turn off. '

    /* cannot light */
    cannotLightMsg = '{You/he} {cannot} light {that dobj/him}. '

    /* cannot burn */
    cannotBurnMsg = '{That dobj/he} {is} not something {you/he} {can} burn. '
    cannotBurnWithMsg =
        '{You/he} {cannot} burn anything with {that iobj/him}. '

    /* cannot burn this specific direct object with this specific iobj */
    cannotBurnDobjWithMsg = '{You/he} {cannot} light {the dobj/him}
                          with {the iobj/him}. '

    /* object is already burning */
    alreadyBurningMsg = '{The dobj/he} {is} already burning. '

    /* cannot extinguish */
    cannotExtinguishMsg = '{You/he} {cannot} extinguish {that dobj/him}. '

    /* cannot pour/pour in/pour on */
    cannotPourMsg = '{That dobj/he} {is} not something {you/he} {can} pour. '
    cannotPourIntoMsg =
        '{You/he} {cannot} pour anything into {that iobj/him}. '
    cannotPourOntoMsg =
        '{You/he} {cannot} pour anything onto {that iobj/him}. '

    /* cannot attach object to object */
    cannotAttachMsg =
        '{You/he} {cannot} attach {that dobj/him} to anything. '
    cannotAttachToMsg =
        '{You/he} {cannot} attach anything to {that iobj/him}. '

    /* cannot attach to self */
    cannotAttachToSelfMsg =
        '{You/he} {cannot} attach {the dobj/him} to {itself}. '

    /* cannot attach because we're already attached to the given object */
    alreadyAttachedMsg =
        '{The dobj/he} {is} already attached to {the iobj/him}. '

    /*
     *   dobj and/or iobj can be attached to certain things, but not to
     *   each other 
     */
    wrongAttachmentMsg =
        '{You/he} {can\'t} attach {that dobj/him} to {the iobj/him}. '

    /* dobj and iobj are attached, but they can't be taken apart */
    wrongDetachmentMsg =
        '{You/he} {can\'t} detach {that dobj/him} from {the iobj/him}. '

    /* must detach the object before proceeding */
    mustDetachMsg(obj)
    {
        gMessageParams(obj);
        return '{You/he} {must} detach {the obj/him} before {it actor/he}
            {can} do that. ';
    }

    /* default message for successful Attachable attachment */
    okayAttachToMsg = 'Done. '

    /* default message for successful Attachable detachment */
    okayDetachFromMsg = 'Done. '

    /* cannot detach object from object */
    cannotDetachMsg = '{You/he} {cannot} detach {that dobj/him}. '
    cannotDetachFromMsg =
        '{You/he} {cannot} detach anything from {that iobj/him}. '

    /* no obvious way to detach a permanent attachment */
    cannotDetachPermanentMsg =
        'There{&rsquo;s| was} no obvious way to detach {that dobj/him}. '

    /* dobj isn't attached to iobj */
    notAttachedToMsg = '{The dobj/he} {is}n&rsquo;t attached
        to {that iobj/him}. '

    /* breaking object would serve no purpose */
    shouldNotBreakMsg =
        'Breaking {that dobj/him} would {|have} serve{|d} no purpose. '

    /* cannot cut that */
    cutNoEffectMsg = '{The iobj/he} {can\'t} seem to cut {the dobj/him}. '

    /* can't use iobj to cut anything */
    cannotCutWithMsg = '{You/he} {can\'t} cut anything with {the iobj/him}. '

    /* cannot climb object */
    cannotClimbMsg =
        '{That dobj/he} {is} not something {you/he} {can} climb. '

    /* object is not openable/closable */
    cannotOpenMsg = '{That dobj/he} {is} not something {you/he} {can} open. '
    cannotCloseMsg =
        '{That dobj/he} {is} not something {you/he} {can} close. '

    /* already open/closed */
    alreadyOpenMsg = '{The dobj/he} {is} already open. '
    alreadyClosedMsg = '{The dobj/he} {is} already closed. '

    /* already locked/unlocked */
    alreadyLockedMsg = '{The dobj/he} {is} already locked. '
    alreadyUnlockedMsg = '{The dobj/he} {is} already unlocked. '

    /* cannot look in container because it's closed */
    cannotLookInClosedMsg = '{The dobj/he} {is} closed. '

    /* object is not lockable/unlockable */
    cannotLockMsg =
        '{That dobj/he} {is} not something {you/he} {can} lock. '
    cannotUnlockMsg =
        '{That dobj/he} {is} not something {you/he} {can} unlock. '

    /* attempting to open a locked object */
    cannotOpenLockedMsg = '{The dobj/he} seem{s/ed} to be locked. '

    /* object requires a key to unlock */
    unlockRequiresKeyMsg =
        '{You/he} seem{s/ed} to need a key to unlock {the dobj/him}. '

    /* object is not a key */
    cannotLockWithMsg =
        '{The iobj/he} {does}n&rsquo;t look suitable for locking that. '
    cannotUnlockWithMsg =
        '{The iobj/he} {does}n&rsquo;t look suitable for unlocking that. '

    /* we don't know how to lock/unlock this */
    unknownHowToLockMsg =
        'It{&rsquo;s| was} not clear how to lock {the dobj/him}. '
    unknownHowToUnlockMsg =
        'It{&rsquo;s| was} not clear how to unlock {the dobj/him}. '

    /* the key (iobj) does not fit the lock (dobj) */
    keyDoesNotFitLockMsg = '{The iobj/he} {does}n&rsquo;t fit the lock. '

    /* found key on keyring */
    foundKeyOnKeyringMsg(ring, key)
    {
        gMessageParams(ring, key);
        return '{You/he} tr{ies/ied} each key on {the ring/him}, and
            {find[s actor]|found} that {the key/he} fit{s/ted} the lock. ';
    }

    /* failed to find a key on keyring */
    foundNoKeyOnKeyringMsg(ring)
    {
        gMessageParams(ring);
        return '{You/he} tr{ies/ied} each key on {the ring/him},
            but {you/he} {can\'t} find anything that fit{s|ted} the lock. ';
    }

    /* not edible/drinkable */
    cannotEatMsg = '{The dobj/he} {does} not appear to be edible. '
    cannotDrinkMsg = '{That dobj/he} {does} not appear to be something
        {you/he} {can} drink. '

    /* cannot clean object */
    cannotCleanMsg =
        '{You/he} wouldn&rsquo;t {|have} know{|n} how to
        clean {that dobj/him}. '
    cannotCleanWithMsg =
        '{You/he} {can\'t} clean anything with {that iobj/him}. '

    /* cannot attach key (dobj) to (iobj) */
    cannotAttachKeyToMsg =
        '{You/he} {cannot} attach {the dobj/him} to {that iobj/him}. '

    /* actor cannot sleep */
    cannotSleepMsg = '{You/he} {does}n&rsquo;t need to sleep right now. '

    /* cannot sit/lie/stand/get on/get out of */
    cannotSitOnMsg =
        '{That dobj/he} {is}n&rsquo;t something {you/he} {can} sit on. '
    cannotLieOnMsg =
        '{That dobj/he} {is}n&rsquo;t something {you/he} {can} lie on. '
    cannotStandOnMsg = '{You/he} {can\'t} stand on {that dobj/him}. '
    cannotBoardMsg = '{You/he} {can\'t} board {that dobj/him}. '
    cannotUnboardMsg = '{You/he} {can\'t} get out of {that dobj/him}. '
    cannotGetOffOfMsg = '{You/he} {can\'t} get off of {that dobj/him}. '

    /* standing on a PathPassage */
    cannotStandOnPathMsg = 'If {you/he} want{s/ed} to follow {the dobj/him},
        just say so. '

    /* cannot sit/lie/stand on something being held */
    cannotEnterHeldMsg =
        '{You/he} {can\'t} do that while {you\'re} holding {the dobj/him}. '

    /* cannot get out (of current location) */
    cannotGetOutMsg = '{You\'re} not in anything {you/he} {can} disembark. '

    /* actor is already in a location */
    alreadyInLocMsg = '{You\'re} already {in dobj}. '

    /* actor is already standing/sitting on/lying on */
    alreadyStandingMsg = '{You\'re} already standing. '
    alreadyStandingOnMsg = '{You\'re} already standing {on dobj}. '
    alreadySittingMsg = '{You\'re} already sitting down. '
    alreadySittingOnMsg = '{You\'re} already sitting {on dobj}. '
    alreadyLyingMsg = '{You\'re} already lying down. '
    alreadyLyingOnMsg = '{You\'re} already lying {on dobj}. '

    /* getting off something you're not on */
    notOnPlatformMsg = '{You\'re} not {on dobj}. '

    /* no room to stand/sit/lie on dobj */
    noRoomToStandMsg =
        'There{&rsquo;s| was} no room for {you/him} to stand {on dobj}. '
    noRoomToSitMsg =
        'There{&rsquo;s| was} no room for {you/him} to sit {on dobj}. '
    noRoomToLieMsg =
        'There{&rsquo;s| was} no room for {you/him} to lie {on dobj}. '

    /* default report for standing up/sitting down/lying down */
    okayPostureChangeMsg(posture)
        { return 'Okay, {you\'re} now ' + posture.participle + '. '; }

    /* default report for standing/sitting/lying in/on something */
    roomOkayPostureChangeMsg(posture, obj)
    {
        gMessageParams(obj);
        return 'Okay, {you\'re} now ' + posture.participle + ' {on obj}. ';
    }

    /* default report for getting off of a platform */
    okayNotStandingOnMsg = 'Okay, {you\'re} no longer {on dobj}. '

    /* cannot fasten/unfasten */
    cannotFastenMsg = '{You/he} {cannot} fasten {the dobj/him}. '
    cannotFastenToMsg =
        '{You/he} {cannot} fasten anything to {the iobj/him}. '
    cannotUnfastenMsg = '{You/he} {cannot} unfasten {the dobj/him}. '
    cannotUnfastenFromMsg =
        '{You/he} {cannot} unfasten anything from {the iobj/him}. '

    /* cannot plug/unplug */
    cannotPlugInMsg = '{You/he} {sees} no way to plug in {the dobj/him}. '
    cannotPlugInToMsg =
        '{You/he} {sees} no way to plug anything into {the iobj/him}. '
    cannotUnplugMsg = '{You/he} {sees} no way to unplug {the dobj/him}. '
    cannotUnplugFromMsg =
        '{You/he} {sees} no way to unplug anything from {the iobj/him}. '

    /* cannot screw/unscrew */
    cannotScrewMsg = '{You/he} {sees} no way to screw {the dobj/him}. '
    cannotScrewWithMsg =
        '{You/he} {cannot} screw anything with {the iobj/him}. '
    cannotUnscrewMsg = '{You/he} {sees} no way to unscrew {the dobj/him}. '
    cannotUnscrewWithMsg =
        '{You/he} {cannot} unscrew anything with {the iobj/him}. '

    /* cannot enter/go through */
    cannotEnterMsg =
        '{That/he dobj} {is} not something {you/he} {can} enter. '
    cannotGoThroughMsg =
        '{That/he dobj} {is} not something {you/he} {can} go through. '
        
    /* can't throw something at itself */
    cannotThrowAtSelfMsg =
        '{You/he} {can\'t} throw {that dobj/him} at {itself}. '

    /* can't throw something at an object inside itself */
    cannotThrowAtContentsMsg = '{You/he} {must} remove {the iobj/him}
        from {the dobj/him} before {it actor/he} {can} do that. '

    /* can't throw through a sense connector */
    cannotThrowThroughMsg(target, loc)
    {
        gMessageParams(target, loc);
        return '{You/he} {cannot} throw anything through {the loc/him}. ';
    }

    /* shouldn't throw something at the floor */
    shouldNotThrowAtFloorMsg =
        '{You/he} should just {|have} put {it dobj/him} down instead. '

    /* THROW <obj> <direction> isn't supported; use THROW AT instead */
    dontThrowDirMsg =
        ('<.parser>You need to be more specific about
            what ' + (gActor.referralPerson == ThirdPerson
                      ? 'you want {the actor/him}' : '')
         + ' to throw {the dobj/him} at.<./parser> ')

    /* thrown object bounces off target (short report) */
    throwHitMsg(projectile, target)
    {
        gMessageParams(projectile, target);
        return '{The projectile/he} hit{[s]|} {the target/him} without any
            obvious effect. ';
    }

    /* thrown object lands on target */
    throwFallMsg(projectile, target)
    {
        gMessageParams(projectile, target);
        return '{The projectile/he} land{s/ed} on {the target/him}. ';
    }

    /* thrown object bounces off target and falls to destination */
    throwHitFallMsg(projectile, target, dest)
    {
        gMessageParams(projectile, target);
        return '{The projectile/he} hit{[s]|} {the target/him}
            without any obvious effect, and {fall[s projectile]|fell} '
            + dest.putInName + '. ';
    }

    /* thrown object falls short of distant target (sentence prefix only) */
    throwShortMsg(projectile, target)
    {
        gMessageParams(projectile, target);
        return '{The projectile/he} {fall[s]|fell} well short of '
               + '{the target/him}. ';
    }
        
    /* thrown object falls short of distant target */
    throwFallShortMsg(projectile, target, dest)
    {
        gMessageParams(projectile, target);
        return '{The projectile/he} {fall[s]|fell} ' + dest.putInName
            + ' well short of {the target/him}. ';
    }

    /* target catches object */
    throwCatchMsg(obj, target)
    {
        return '\^' + target.theName + ' '
            + tSel('catch' + target.verbEndingEs, 'caught')
            + ' ' + obj.theNameObj + '. ';
    }

    /* we're not a suitable target for THROW TO (because we're not an NPC) */
    cannotThrowToMsg = '{You/he} {cannot} throw anything to {an iobj/him}. '

    /* target does not want to catch anything */
    willNotCatchMsg(catcher)
    {
        return '\^' + catcher.nameDoes
            + 'n&rsquo;t look like ' + catcher.itNom + ' want'
            + catcher.verbEndingSEd + ' to catch anything. ';
    }

    /* cannot kiss something */
    cannotKissMsg = 'Kissing {the dobj/him} {has|had} no obvious effect. '

    /* person uninterested in being kissed */
    cannotKissActorMsg
        = '{The dobj/he} probably wouldn&rsquo;t {|have} like{|d} that. '

    /* cannot kiss yourself */
    cannotKissSelfMsg = '{You/he} {cannot} kiss {yourself}. '

    /* it is now dark at actor's location */
    newlyDarkMsg = 'It {is|was} now pitch black. '
;

/*
 *   Non-player character verb messages.  By default, we inherit all of
 *   the messages defined for the player character, but we override some
 *   that must be rephrased slightly to make sense for NPC's.
 */
npcActionMessages: playerActionMessages
    /* "wait" */
    timePassesMsg = '{You/he} wait{s/ed}... '

    /* trying to move a Fixture/Immovable */
    cannotMoveFixtureMsg = '{You/he} {cannot} move {that dobj/him}. '
    cannotMoveImmovableMsg = '{You/he} {cannot} move {that dobj/him}. '

    /* trying to take/move/put a Heavy object */
    cannotTakeHeavyMsg =
        '{That dobj/he} {is} too heavy for {you/him} to take. '
    cannotMoveHeavyMsg =
        '{That dobj/he} {is} too heavy for {you/him} to move. '
    cannotPutHeavyMsg =
        '{That dobj/he} {is} too heavy for {you/him} to move. '

    /* trying to move a component object */
    cannotMoveComponentMsg(loc)
    {
        return '{You/he} {cannot} do that because {the dobj/he} {is} part
            of ' + loc.theNameObj + '. ';
    }

    /* default successful 'take' response */
    okayTakeMsg = '{You/he} {take[s]|took} {the dobj/him}. '

    /* default successful 'drop' response */
    okayDropMsg = '{You/he} put{[s]|} down {the dobj/him}. '

    /* default successful 'put in' response */
    okayPutInMsg = '{You/he} put{[s]|} {the dobj/him} in {the iobj/him}. '

    /* default successful 'put on' response */
    okayPutOnMsg = '{You/he} put{[s]|} {the dobj/him} on {the iobj/him}. '

    /* default successful 'put under' response */
    okayPutUnderMsg =
        '{You/he} put{[s]|} {the dobj/him} under {the iobj/him}. '

    /* default successful 'put behind' response */
    okayPutBehindMsg =
        '{You/he} put{[s]|} {the dobj/him} behind {the iobj/him}. '

    /* default succesful response to 'wear obj' */
    okayWearMsg =
        '{You/he} put{[s]|} on {the dobj/him}. '

    /* default successful response to 'doff obj' */
    okayDoffMsg = '{You/he} {take[s]|took} off {the dobj/him}. '

    /* default successful responses to open/close */
    okayOpenMsg = '{You/he} open{s/ed} {the dobj/him}. '
    okayCloseMsg = '{You/he} close{s/d} {the dobj/him}. '

    /* default successful responses to lock/unlock */
    okayLockMsg = '{You/he} lock{s/ed} {the dobj/him}. '
    okayUnlockMsg = '{You/he} unlock{s/ed} {the dobj/him}. '

    /* push/pull/move with no effect */
    pushNoEffectMsg = '{You/he} tr{ies/ied} to push {the dobj/him}, with no '
                      + 'obvious effect. '
    pullNoEffectMsg = '{You/he} tr{ies/ied} to pull {the dobj/him}, with no '
                      + 'obvious effect. '
    moveNoEffectMsg = '{You/he} tr{ies/ied} to move {the dobj/him}, with no '
                      + 'obvious effect. '
    moveToNoEffectMsg = '{You/he} {leaves} {the dobj/he} where {it/he} {is}. '

    whereToGoMsg =
        'You&rsquo;ll have to say which way {you/he} should {|have} go{|ne}. '

    /* object is too large for container */
    tooLargeForContainerMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{You/he} {cannot} do that because {the obj/he} {is}
            too large for {the cont/him}. ';
    }

    /* object is too large for underside */
    tooLargeForUndersideMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{You/he} {cannot} do that because {the obj/he}
            {won\'t} fit under {the cont/him}. ';
    }

    /* object is too large to fit behind something */
    tooLargeForRearMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{You/he} {cannot} do that because {the obj/he}
            {won\'t} fit behind {the cont/him}. ';
    }

    /* container doesn't have room for object */
    containerTooFullMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{You/he} {cannot} do that because {the cont/he} {is}
            already too full to hold {the obj/him}. ';
    }

    /* surface doesn't have room for object */
    surfaceTooFullMsg(obj, cont)
    {
        gMessageParams(obj, cont);
        return '{You/he} {cannot} do that because there{\'s| was}
            no room for {the obj/him} on {the cont/him}. ';
    }

    /* the dobj doesn't fit on this keyring */
    objNotForKeyringMsg = '{You/he} {cannot} do that because {that dobj/he}
        {does}n&rsquo;t fit on {the iobj/him}. '

    /* taking dobj from iobj, but dobj isn't in iobj */
    takeFromNotInMsg = '{You/he} {cannot} do that because
        {the dobj/he} {is}n&rsquo;t in {that iobj/him}. '

    /* taking dobj from surface, but dobj isn't on iobj */
    takeFromNotOnMsg = '{You/he} {cannot} do that because
        {the dobj/he} {is}n&rsquo;t on {that iobj/him}. '

    /* taking dobj under something, but dobj isn't under iobj */
    takeFromNotUnderMsg = '{You/he} {cannot} do that because
        {the dobj/he} {is}n&rsquo;t under {that iobj/him}. '

    /* taking dobj from behind something, but dobj isn't behind iobj */
    takeFromNotBehindMsg = '{You/he} {cannot} do that because
        {the dobj/he} {is}n&rsquo;t behind {that iobj/him}. '

    /* cannot jump off (with no direct object) from here */
    cannotJumpOffHereMsg = 'There{&rsquo;s| was} nowhere for
        {you/him} to jump. '

    /* should not break object */
    shouldNotBreakMsg = '{You/he} {does}n&rsquo;t want to
        break {that dobj/him}. '

    /* report for standing up/sitting down/lying down */
    okayPostureChangeMsg(posture)
        { return '{You/he} ' + posture.msgVerbI + '. '; }

    /* report for standing/sitting/lying in/on something */
    roomOkayPostureChangeMsg(posture, obj)
    {
        gMessageParams(obj);
        return '{You/he} ' + posture.msgVerbT + ' {on obj}. ';
    }

    /* report for getting off a platform */
    okayNotStandingOnMsg = '{You/he} {get[s]|got} {offof dobj}. '

    /* default 'turn to' acknowledgment */
    okayTurnToMsg(val)
        { return '{You/he} turn{s/ed} {the dobj/him} to ' + val + '. '; }

    /* default 'push button' acknowledgment */
    okayPushButtonMsg = '{You/he} push{es/ed} {the dobj/him}. '

    /* default acknowledgment for switching on/off */
    okayTurnOnMsg = '{You/he} turn{s/ed} {the dobj/him} on. '
    okayTurnOffMsg = '{You/he} turn{s/ed} {the dobj/him} off. '

    /* the key (iobj) does not fit the lock (dobj) */
    keyDoesNotFitLockMsg = '{You/he} tr{ies/ied} {the iobj/he}, but
                         {it iobj/he} {does}n&rsquo;t fit the lock. '

    /* acknowledge entering "follow" mode */
    okayFollowModeMsg = '<q>Okay, I will follow {the dobj/him}.</q> '

    /* note that we're already in "follow" mode */
    alreadyFollowModeMsg = '<q>I\'m already following {the dobj/him}.</q> '

    /* extinguishing a candle */
    okayExtinguishCandleMsg = '{You/he} extinguish{es/ed} {the dobj/him}. '

    /* acknowledge attachment */
    okayAttachToMsg =
        '{You/he} attach{es/ed} {the dobj/him} to {the iobj/him}. '

    /* acknowledge detachment */
    okayDetachFromMsg =
        '{You/he} detach{es/ed} {the dobj/him} from {the iobj/him}. '

    /*
     *   the PC's responses to conversational actions applied to oneself
     *   need some reworking for NPC's 
     */
    cannotTalkToSelfMsg = '{You/he} {won\'t} accomplish anything talking
        to {yourself/himself}. '
    cannotAskSelfMsg = '{You/he} {won\'t} accomplish anything talking
        to {yourself/himself}. '
    cannotAskSelfForMsg = '{You/he} {won\'t} accomplish anything talking
        to {yourself/himself}. '
    cannotTellSelfMsg = '{You/he} {won\'t} accomplish anything talking
        to {yourself/himself}. '
    cannotGiveToSelfMsg = '{You/he} {won\'t} accomplish anything
        giving {the dobj/him} to {yourself/himself}. '
    cannotShowToSelfMsg = '{You/he} {won\'t} accomplish anything
        showing {the dobj/him} to {yourself/himself}. '
;

/* ------------------------------------------------------------------------ */
/*
 *   Standard tips
 */

scoreChangeTip: Tip
    "If you&rsquo;d prefer not to be notified about score changes in the
    future, type <<aHref('notify off', 'NOTIFY OFF', 'Turn off score
    notifications')>>."
;

footnotesTip: Tip
    "A number in [square brackets] like the one above refers to a footnote,
    which you can read by typing FOOTNOTE followed by the number:
    <<aHref('footnote 1', 'FOOTNOTE 1', 'Show footnote [1]')>>, for example.
    Footnotes usually contain added background information that might be
    interesting but isn&rsquo;t essential to the story. If you&rsquo;d
    prefer not to see footnotes at all, you can control their appearance by
    typing <<aHref('footnotes', 'FOOTNOTES', 'Control footnote
    appearance')>>."
;

oopsTip: Tip
    "If this was an accidental misspelling, you can correct it by typing
    OOPS followed by the corrected word now. Any time the story points out an
    unknown word, you can correct a misspelling using OOPS as your next
    command."
;

fullScoreTip: Tip
    "To see a detailed accounting of your score, type
    <<aHref('full score', 'FULL SCORE')>>."
;

exitsTip: Tip
    "You can control the exit listings with the EXITS command.
    <<aHref('exits status', 'EXITS STATUS',
            'Turn on status line exit listings')>>
    shows the exit list in the status line,
    <<aHref('exits look', 'EXITS LOOK', 'List exits in room descriptions')>>
    shows a full exit list in each room description,
    <<aHref('exits on', 'EXITS ON', 'Turn on all exit lists')>>
    shows both, and
    <<aHref('exits off', 'EXITS OFF', 'Turn off all exit lists')>>
    turns off both kinds of exit lists."
;

undoTip: Tip
    "If this didn't turn out quite the way you expected, note that you
    can always take back a turn by typing <<aHref('undo', 'UNDO',
        'Take back the most recent command')>>.  You can even use
    UNDO repeatedly to take back several turns in a row. "
;


/* ------------------------------------------------------------------------ */
/*
 *   Listers
 */

/*
 *   The basic "room lister" object - this is the object that we use by
 *   default with showList() to construct the listing of the portable
 *   items in a room when displaying the room's description.  
 */
roomLister: Lister
    /* show the prefix/suffix in wide mode */
    showListPrefixWide(itemCount, pov, parent) { "{You/he} {sees} "; }
    showListSuffixWide(itemCount, pov, parent) { " {|t}here. "; }

    /* show the tall prefix */
    showListPrefixTall(itemCount, pov, parent) { "{You/he} {sees}:"; }
;

/*
 *   The basic room lister for dark rooms 
 */
darkRoomLister: Lister
    showListPrefixWide(itemCount, pov, parent)
        { "In the darkness {you/he} {can} see "; }

    showListSuffixWide(itemCount, pov, parent) { ". "; }

    showListPrefixTall(itemCount, pov, parent)
        { "In the darkness {you/he} {sees}:"; }
;

/*
 *   A "remote room lister".  This is used to describe the contents of an
 *   adjoining room.  For example, if an actor is standing in one room,
 *   and can see into a second top-level room through a window, we'll use
 *   this lister to describe the objects the actor can see through the
 *   window. 
 */
class RemoteRoomLister: Lister
    construct(room) { remoteRoom = room; }
    
    showListPrefixWide(itemCount, pov, parent)
        { "\^<<remoteRoom.inRoomName(pov)>>, {you/he} {sees} "; }
    showListSuffixWide(itemCount, pov, parent)
        { ". "; }

    showListPrefixTall(itemCount, pov, parent)
        { "\^<<remoteRoom.inRoomName(pov)>>, {you/he} {sees}:"; }

    /* the remote room we're viewing */
    remoteRoom = nil
;

/*
 *   A simple customizable room lister.  This can be used to create custom
 *   listers for things like remote room contents listings.  We act just
 *   like any ordinary room lister, but we use custom prefix and suffix
 *   strings provided during construction.  
 */
class CustomRoomLister: Lister
    construct(prefix, suffix)
    {
        prefixStr = prefix;
        suffixStr = suffix;
    }

    showListPrefixWide(itemCount, pov, parent) { "<<prefixStr>> "; }
    showListSuffixWide(itemCount, pov, parent) { "<<suffixStr>> "; }
    showListPrefixTall(itemCount, pov, parent) { "<<prefixStr>>:"; }

    /* our prefix and suffix strings */
    prefixStr = nil
    suffixStr = nil
;

/*
 *   Single-list inventory lister.  This shows the inventory listing as a
 *   single list, with worn items mixed in among the other inventory items
 *   and labeled "(being worn)".  
 */
actorSingleInventoryLister: InventoryLister
    showListPrefixWide(itemCount, pov, parent)
        { "<<buildSynthParam('The/he', parent)>> {is} carrying "; }
    showListSuffixWide(itemCount, pov, parent)
        { ". "; }

    showListPrefixTall(itemCount, pov, parent)
        { "<<buildSynthParam('The/he', parent)>> {is} carrying:"; }
    showListContentsPrefixTall(itemCount, pov, parent)
        { "<<buildSynthParam('A/he', parent)>>, who {is} carrying:"; }

    showListEmpty(pov, parent)
        { "<<buildSynthParam('The/he', parent)>> {is} empty-handed. "; }
;

/*
 *   Standard inventory lister for actors - this will work for the player
 *   character and NPC's as well.  This lister uses a "divided" format,
 *   which segregates the listing into items being carried and items being
 *   worn.  We'll combine the two lists into a single sentence if the
 *   overall list is short, otherwise we'll show two separate sentences for
 *   readability.  
 */
actorInventoryLister: DividedInventoryLister
    /*
     *   Show the combined inventory listing, putting together the raw
     *   lists of the items being carried and the items being worn. 
     */
    showCombinedInventoryList(parent, carrying, wearing)
    {
        /* if one or the other sentence is empty, the format is simple */
        if (carrying == '' && wearing == '')
        {
            /* the parent is completely empty-handed */
            showInventoryEmpty(parent);
        }
        else if (carrying == '')
        {
            /* the whole list is being worn */
            showInventoryWearingOnly(parent, wearing);
        }
        else if (wearing == '')
        {
            /* the whole list is being carried */
            showInventoryCarryingOnly(parent, carrying);
        }
        else
        {
            /*
             *   Both listings are populated.  Count the number of
             *   comma-separated or semicolon-separated phrases in each
             *   list.  This will give us an estimate of the grammatical
             *   complexity of each list.  If we have very short lists, a
             *   single sentence will be easier to read; if the lists are
             *   long, we'll show the lists in separate sentences.  
             */
            if (countPhrases(carrying) + countPhrases(wearing)
                <= singleSentenceMaxNouns)
            {
                /* short enough: use a single-sentence format */
                showInventoryShortLists(parent, carrying, wearing);
            }
            else
            {
                /* long: use a two-sentence format */
                showInventoryLongLists(parent, carrying, wearing);
            }
        }
    }

    /*
     *   Count the noun phrases in a string.  We'll count the number of
     *   elements in the list as indicated by commas and semicolons.  This
     *   might not be a perfect count of the actual number of noun phrases,
     *   since we could have commas setting off some other kind of clauses,
     *   but it nonetheless will give us a good estimate of the overall
     *   complexity of the text, which is what we're really after.  The
     *   point is that we want to break up the listings if they're long,
     *   but combine them into a single sentence if they're short.  
     */
    countPhrases(txt)
    {
        local cnt;
        
        /* if the string is empty, there are no phrases at all */
        if (txt == '')
            return 0;

        /* a non-empty string has at least one phrase */
        cnt = 1;

        /* scan for commas and semicolons */
        for (local startIdx = 1 ;;)
        {
            local idx;
            
            /* find the next phrase separator */
            idx = rexSearch(phraseSepPat, txt, startIdx);

            /* if we didn't find it, we're done */
            if (idx == nil)
                break;

            /* count it */
            ++cnt;

            /* continue scanning after the separator */
            startIdx = idx[1] + idx[2];
        }

        /* return the count */
        return cnt;
    }

    phraseSepPat = static new RexPattern(',(?! and )|;| and |<rparen>')

    /*
     *   Once we've made up our mind about the format, we'll call one of
     *   these methods to show the final sentence.  These are all separate
     *   methods so that the individual formats can be easily tweaked
     *   without overriding the whole combined-inventory-listing method. 
     */
    showInventoryEmpty(parent)
    {
        /* empty inventory */
        "<<buildSynthParam('The/he', parent)>> {is} empty-handed. ";
    }
    showInventoryWearingOnly(parent, wearing)
    {
        /* we're carrying nothing but wearing some items */
        "<<buildSynthParam('The/he', parent)>> {is} carrying nothing,
        and {is} wearing <<wearing>>. ";
    }
    showInventoryCarryingOnly(parent, carrying)
    {
        /* we have only carried items to report */
        "<<buildSynthParam('The/he', parent)>> {is} carrying <<carrying>>. ";
    }
    showInventoryShortLists(parent, carrying, wearing)
    {
        local nm = gSynthMessageParam(parent);
        
        /* short lists - combine carried and worn in a single sentence */
        "<<buildParam('The/he', nm)>> {is} carrying <<carrying>>,
        and <<buildParam('it\'s', nm)>>{subj} wearing <<wearing>>. ";
    }
    showInventoryLongLists(parent, carrying, wearing)
    {
        local nm = gSynthMessageParam(parent);

        /* long lists - show carried and worn in separate sentences */
        "<<buildParam('The/he', nm)>> {is} carrying <<carrying>>.
        <<buildParam('It\'s', nm)>> wearing <<wearing>>. ";
    }

    /*
     *   For 'tall' listings, we'll use the standard listing style, so we
     *   need to provide the framing messages for the tall-mode listing.  
     */
    showListPrefixTall(itemCount, pov, parent)
        { "<<buildSynthParam('The/he', parent)>> {is} carrying:"; }
    showListContentsPrefixTall(itemCount, pov, parent)
        { "<<buildSynthParam('A/he', parent)>>, who {is} carrying:"; }
    showListEmpty(pov, parent)
        { "<<buildSynthParam('The/he', parent)>> {is} empty-handed. "; }
;

/*
 *   Special inventory lister for non-player character descriptions - long
 *   form lister.  This is used to display the inventory of an NPC as part
 *   of the full description of the NPC.
 *   
 *   This long form lister is meant for actors with lengthy descriptions.
 *   We start the inventory listing on a new line, and use the actor's
 *   full name in the list preface.  
 */
actorHoldingDescInventoryListerLong: actorInventoryLister
    showInventoryEmpty(parent)
    {
        /* empty inventory - saying nothing in an actor description */
    }
    showInventoryWearingOnly(parent, wearing)
    {
        /* we're carrying nothing but wearing some items */
        "<.p><<buildSynthParam('The/he', parent)>> {is}
        wearing <<wearing>>. ";
    }
    showInventoryCarryingOnly(parent, carrying)
    {
        /* we have only carried items to report */
        "<.p><<buildSynthParam('The/he', parent)>> {is} carrying
        <<carrying>>. ";
    }
    showInventoryShortLists(parent, carrying, wearing)
    {
        local nm = gSynthMessageParam(parent);

        /* short lists - combine carried and worn in a single sentence */
        "<.p><<buildParam('The/he', nm)>> {is} carrying <<carrying>>,
        and <<buildParam('it\'s', nm)>>{subj} wearing <<wearing>>. ";
    }
    showInventoryLongLists(parent, carrying, wearing)
    {
        local nm = gSynthMessageParam(parent);

        /* long lists - show carried and worn in separate sentences */
        "<.p><<buildParam('The/he', nm)>> {is} carrying <<carrying>>.
        <<buildParam('It\'s', nm)>> wearing <<wearing>>. ";
    }
;

/* short form of non-player character description inventory lister */
actorHoldingDescInventoryListerShort: actorInventoryLister
    showInventoryEmpty(parent)
    {
        /* empty inventory - saying nothing in an actor description */
    }
    showInventoryWearingOnly(parent, wearing)
    {
        /* we're carrying nothing but wearing some items */
        "<<buildSynthParam('It/he', parent)>> {is} wearing <<wearing>>. ";
    }
    showInventoryCarryingOnly(parent, carrying)
    {
        /* we have only carried items to report */
        "<<buildSynthParam('It/he', parent)>> {is} carrying <<carrying>>. ";
    }
    showInventoryShortLists(parent, carrying, wearing)
    {
        local nm = gSynthMessageParam(parent);

        /* short lists - combine carried and worn in a single sentence */
        "<<buildParam('It/he', nm)>> {is} carrying <<carrying>>, and
        <<buildParam('it\'s', nm)>>{subj} wearing <<wearing>>. ";
    }
    showInventoryLongLists(parent, carrying, wearing)
    {
        local nm = gSynthMessageParam(parent);

        /* long lists - show carried and worn in separate sentences */
        "<<buildParam('It/he', nm)>> {is} carrying <<carrying>>.
        <<buildParam('It\'s', nm)>> wearing <<wearing>>. ";
    }
;

/*
 *   Base contents lister for things.  This is used to display the contents
 *   of things shown in room and inventory listings; we subclass this for
 *   various purposes 
 */
class BaseThingContentsLister: Lister
    showListPrefixWide(itemCount, pov, parent)
        { "\^<<parent.nameVerb('contain')>> "; }
    showListSuffixWide(itemCount, pov, parent)
        { ". "; }
    showListPrefixTall(itemCount, pov, parent)
        { "\^<<parent.nameVerb('contain')>>:"; }
    showListContentsPrefixTall(itemCount, pov, parent)
        { "<<parent.aName>>, which contain<<parent.verbEndingSEd>>:"; }
;

/*
 *   Contents lister for things.  This is used to display the second-level
 *   contents lists for things listed in the top-level list for a room
 *   description, inventory listing, or object description.  
 */
thingContentsLister: ContentsLister, BaseThingContentsLister
;

/*
 *   Contents lister for descriptions of things - this is used to display
 *   the contents of a thing as part of the long description of the thing
 *   (in response to an "examine" command); it differs from a regular
 *   thing contents lister in that we use a pronoun to refer to the thing,
 *   rather than its full name, since the full name was probably just used
 *   in the basic long description.  
 */
thingDescContentsLister: DescContentsLister, BaseThingContentsLister
    showListPrefixWide(itemCount, pov, parent)
        { "\^<<parent.itVerb('contain')>> "; }
;

/*
 *   Contents lister for openable things.
 */
openableDescContentsLister: thingDescContentsLister
    showListEmpty(pov, parent)
    {
        "\^<<parent.openStatus>>. ";
    }
    showListPrefixWide(itemCount, pov, parent)
    {
        "\^<<parent.openStatus>>, and contain<<parent.verbEndingSEd>> ";
    }
;

/*
 *   Base contents lister for "LOOK <PREP>" commands (LOOK IN, LOOK UNDER,
 *   LOOK BEHIND, etc).  This can be subclasses for the appropriate LOOK
 *   <PREP> command matching the container type - LOOK UNDER for
 *   undersides, LOOK BEHIND for rear containers, etc.  To use this class,
 *   combine it via multiple inheritance with the appropriate
 *   Base<Prep>ContentsLister for the preposition type.  
 */
class LookWhereContentsLister: DescContentsLister
    showListEmpty(pov, parent)
    {
        /* show a default message indicating the surface is empty */
        gMessageParams(parent);
        defaultDescReport('{You/he} {sees} nothing ' + parent.objInPrep
                          + ' {the parent/him}. ');
    }
;

/*
 *   Contents lister for descriptions of things whose contents are
 *   explicitly inspected ("look in").  This differs from a regular
 *   contents lister in that we explicitly say that the object is empty if
 *   it's empty.
 */
thingLookInLister: LookWhereContentsLister, BaseThingContentsLister
    showListEmpty(pov, parent)
    {
        /*
         *   Indicate that the list is empty, but make this a default
         *   descriptive report.  This way, if we report any special
         *   descriptions for items contained within the object, we'll
         *   suppress this default description that there's nothing to
         *   describe, which is clearly wrong if there are
         *   specially-described contents. 
         */
        gMessageParams(parent);
        defaultDescReport('{You/he} {sees} nothing unusual
            in {the parent/him}. ');
    }
;

/*
 *   Default contents lister for newly-revealed objects after opening a
 *   container.  
 */
openableOpeningLister: BaseThingContentsLister
    showListEmpty(pov, parent) { }
    showListPrefixWide(itemCount, pov, parent)
    {
        /*
         *   This list is, by default, generated as a replacement for the
         *   default "Opened" message in response to an OPEN command.  We
         *   therefore need different messages for PC and NPC actions,
         *   since this serves as the description of the entire action.
         *   
         *   Note that if you override the Open action response for a given
         *   object, you might want to customize this lister as well, since
         *   the message we generate (especially for an NPC action)
         *   presumes that we'll be the only response the command.  
         */
        gMessageParams(pov, parent);
        if (pov.isPlayerChar())
            "Opening {the parent/him} reveal{s|ed} ";
        else
            "{The pov/he} open{s/ed} {the parent/him}, revealing ";
    }
;

/*
 *   Base contents lister.  This class handles contents listings for most
 *   kinds of specialized containers - Surfaces, RearConainers,
 *   RearSurfaces, and Undersides.  The main variation in the contents
 *   listings for these various types of containers is simply the
 *   preposition that's used to describe the relationship between the
 *   container and the contents, and for this we can look to the objInPrep
 *   property of the container.  
 */
class BaseContentsLister: Lister
    showListPrefixWide(itemCount, pov, parent)
    {
        "\^<<parent.objInPrep>> <<parent.theNameObj>>
        <<itemCount == 1 ? tSel('is', 'was') : tSel('are', 'were')>> ";
    }
    showListSuffixWide(itemCount, pov, parent)
    {
        ". ";
    }
    showListPrefixTall(itemCount, pov, parent)
    {
        "\^<<parent.objInPrep>> <<parent.theNameObj>>
        <<itemCount == 1 ? tSel('is', 'was') : tSel('are', 'were')>>:";
    }
    showListContentsPrefixTall(itemCount, pov, parent)
    {
        "<<parent.aName>>, <<parent.objInPrep>> which
        <<itemCount == 1 ? tSel('is', 'was') : tSel('are', 'were')>>:";
    }
;


/*
 *   Base class for contents listers for a surface 
 */
class BaseSurfaceContentsLister: BaseContentsLister
;

/*
 *   Contents lister for a surface 
 */
surfaceContentsLister: ContentsLister, BaseSurfaceContentsLister
;

/*
 *   Contents lister for explicitly looking in a surface 
 */
surfaceLookInLister: LookWhereContentsLister, BaseSurfaceContentsLister
;

/*
 *   Contents lister for a surface, used in a long description. 
 */
surfaceDescContentsLister: DescContentsLister, BaseSurfaceContentsLister
;

/*
 *   Contents lister for room parts
 */
roomPartContentsLister: surfaceContentsLister
    isListed(obj)
    {
        /* list the object if it's listed in the room part */
        return obj.isListedInRoomPart(part_);
    }

    /* the part I'm listing */
    part_ = nil
;

/*
 *   contents lister for room parts, used in a long description 
 */
roomPartDescContentsLister: surfaceDescContentsLister
    isListed(obj)
    {
        /* list the object if it's listed in the room part */
        return obj.isListedInRoomPart(part_);
    }

    part_ = nil
;

/*
 *   Look-in lister for room parts.  Most room parts act like surfaces
 *   rather than containers, so base this lister on the surface lister.  
 */
roomPartLookInLister: surfaceLookInLister
    isListed(obj)
    {
        /* list the object if it's listed in the room part */
        return obj.isListedInRoomPart(part_);
    }

    part_ = nil
;
                         
/*
 *   Base class for contents listers for an Underside.  
 */
class BaseUndersideContentsLister: BaseContentsLister
;

/* basic contents lister for an Underside */
undersideContentsLister: ContentsLister, BaseUndersideContentsLister
;

/* contents lister for explicitly looking under an Underside */
undersideLookUnderLister: LookWhereContentsLister, BaseUndersideContentsLister
;

/* contents lister for moving an Underside and abandoning its contents */
undersideAbandonContentsLister: undersideLookUnderLister
    showListEmpty(pov, parent) { }
    showListPrefixWide(itemCount, pov, parent)
        { "Moving <<parent.theNameObj>> reveal<<tSel('s', 'ed')>> "; }
    showListSuffixWide(itemCount, pov, parent)
        { " underneath. "; }
    showListPrefixTall(itemCount, pov, parent)
        { "Moving <<parent.theNameObj>> reveal<<tSel('s', 'ed')>>:"; }
;
 
/* contents lister for an Underside, used in a long description */
undersideDescContentsLister: DescContentsLister, BaseUndersideContentsLister
    showListPrefixWide(itemCount, pov, parent)
    {
        "Under <<parent.theNameObj>>
        <<itemCount == 1 ? tSel('is', 'was')
                         : tSel('are', 'were')>> ";
    }
;

/*
 *   Base class for contents listers for an RearContainer or RearSurface 
 */
class BaseRearContentsLister: BaseContentsLister
;

/* basic contents lister for a RearContainer or RearSurface */
rearContentsLister: ContentsLister, BaseRearContentsLister
;

/* contents lister for explicitly looking behind a RearContainer/Surface */
rearLookBehindLister: LookWhereContentsLister, BaseRearContentsLister
;
 
/* lister for moving a RearContainer/Surface and abandoning its contents */
rearAbandonContentsLister: undersideLookUnderLister
    showListEmpty(pov, parent) { }
    showListPrefixWide(itemCount, pov, parent)
        { "Moving <<parent.theNameObj>> reveal<<tSel('s', 'ed')>> "; }
    showListSuffixWide(itemCount, pov, parent)
        { " behind <<parent.itObj>>. "; }
    showListPrefixTall(itemCount, pov, parent)
        { "Moving <<parent.theNameObj>> reveal<<tSel('s', 'ed')>>:"; }
;
 
/* long-description contents lister for a RearContainer/Surface */
rearDescContentsLister: DescContentsLister, BaseRearContentsLister
    showListPrefixWide(itemCount, pov, parent)
    {
        "Behind <<parent.theNameObj>>
        <<itemCount == 1 ? tSel('is', 'was')
                         : tSel('are', 'were')>> ";
    }
;


/*
 *   Base class for specialized in-line contents listers.  This shows the
 *   list in the form "(<prep> which is...)", with the preposition obtained
 *   from the container's objInPrep property.  
 */
class BaseInlineContentsLister: ContentsLister
    showListEmpty(pov, parent) { }
    showListPrefixWide(cnt, pov, parent)
    {
        " (<<parent.objInPrep>> which <<
          cnt == 1 ? tSel('is', 'was') : tSel('are', 'were')>> ";
    }
    showListSuffixWide(itemCount, pov, parent)
        { ")"; }
;

/*
 *   Contents lister for a generic in-line list entry.  We customize the
 *   wording slightly here: rather than saying "(in which...)" as the base
 *   class would, we use the slightly more readable "(which contains...)".
 */
inlineListingContentsLister: BaseInlineContentsLister
    showListPrefixWide(cnt, pov, parent)
        { " (which contain<<parent.verbEndingSEd>> "; }
;

/* in-line contents lister for a surface */
surfaceInlineContentsLister: BaseInlineContentsLister
;

/* in-line contents lister for an Underside */
undersideInlineContentsLister: BaseInlineContentsLister
;

/* in-line contents lister for a RearContainer/Surface */
rearInlineContentsLister: BaseInlineContentsLister
;

/*
 *   Contents lister for keyring list entry.  This is used to display a
 *   keyring's contents in-line with the name of the keyring,
 *   parenthetically. 
 */
keyringInlineContentsLister: inlineListingContentsLister
    showListPrefixWide(cnt, pov, parent)
        { " (with "; }
    showListSuffixWide(cnt, pov, parent)
        { " attached)"; }
;


/*
 *   Contents lister for "examine <keyring>" 
 */
keyringExamineContentsLister: DescContentsLister
    showListEmpty(pov, parent)
    {
        "\^<<parent.nameIs>> empty. ";
    }
    showListPrefixWide(cnt, pov, parent)
    {
        "Attached to <<parent.theNameObj>>
        <<cnt == 1 ? tSel('is', 'was')
                   : tSel('are', 'were')>> ";
    }
    showListSuffixWide(itemCount, pov, parent)
    {
        ". ";
    }
;

/*
 *   Lister for actors aboard a traveler.  This is used to list the actors
 *   on board a vehicle when the vehicle arrives or departs a location.  
 */
aboardVehicleLister: Lister
    showListPrefixWide(itemCount, pov, parent)
        { " (carrying "; }
    showListSuffixWide(itemCount, pov, parent)
        { ")"; }

    /* list anything whose isListedAboardVehicle returns true */
    isListed(obj) { return obj.isListedAboardVehicle; }
;

/*
 *   A simple lister to show the objects to which a given Attachable
 *   object is attached.  This shows the objects which have symmetrical
 *   attachment relationships to the given parent object, or which are
 *   "major" items to which the parent is attached.  
 */
class SimpleAttachmentLister: Lister
    construct(parent) { parent_ = parent; }
    
    showListEmpty(pov, parent)
        { /* say nothing when there are no attachments */ }
    
    showListPrefixWide(cnt, pov, parent)
        { "<.p>\^<<parent.nameIs>> attached to "; }
    showListSuffixWide(cnt, pov, parent)
        { ". "; }

    /* ask the parent if we should list each item */
    isListed(obj) { return parent_.isListedAsAttachedTo(obj); }

    /*
     *   the parent object - this is the object whose attachments are being
     *   listed 
     */
    parent_ = nil
;

/*
 *   The "major" attachment lister.  This lists the objects which are
 *   attached to a given parent Attachable, and for which the parent is
 *   the "major" item in the relationship.  The items in the list are
 *   described as being attached to the parent.  
 */
class MajorAttachmentLister: SimpleAttachmentLister
    showListPrefixWide(cnt, pov, parent) { "<.p>\^"; }
    showListSuffixWide(cnt, pov, parent)
    {
        " <<cnt == 1 ? tSel('is', 'was')
                     : tSel('are', 'were')>>
        attached to <<parent.theNameObj>>. ";
    }

    /* ask the parent if we should list each item */
    isListed(obj) { return parent_.isListedAsMajorFor(obj); }
;

/*
 *   Finish Options lister.  This lister is used to offer the player
 *   options in finishGame(). 
 */
finishOptionsLister: Lister
    showListPrefixWide(cnt, pov, parent)
    {
        "<.p>Would you like to ";
    }
    showListSuffixWide(cnt, pov, parent)
    {
        /* end the question, add a blank line, and show the ">" prompt */
        "?\b&gt;";
    }
    
    isListed(obj) { return obj.isListed; }
    listCardinality(obj) { return 1; }
    
    showListItem(obj, options, pov, infoTab)
    {
        /* obj is a FinishOption object; show its description */
        obj.desc;
    }
    
    showListSeparator(options, curItemNum, totalItems)
    {
        /*
         *   for the last separator, show "or" rather than "and"; for
         *   others, inherit the default handling 
         */
        if (curItemNum + 1 == totalItems)
        {
            if (totalItems == 2)
                " or ";
            else
                ", or ";
        }
        else
            inherited(options, curItemNum, totalItems);
    }
;

/*
 *   Equivalent list state lister.  This shows a list of state names for a
 *   set of otherwise indistinguishable items.  We show the state names in
 *   parentheses, separated by commas only (i.e., no "and" separating the
 *   last two items); we use this less verbose format so that we blend
 *   into the larger enclosing list more naturally.
 *   
 *   The items to be listed are EquivalentStateInfo objects.  
 */
equivalentStateLister: Lister
    showListPrefixWide(cnt, pov, parent) { " ("; }
    showListSuffixWide(cnt, pov, parent) { ")"; }
    isListed(obj) { return true; }
    listCardinality(obj) { return 1; }
    showListItem(obj, options, pov, infoTab)
    {
        "<<spellIntBelow(obj.getEquivCount(), 100)>> <<obj.getName()>>";
    }
    showListSeparator(options, curItemNum, totalItems)
    {
        if (curItemNum < totalItems)
            ", ";
    }
;

/* in case the exits module isn't included in the build */
property destName_, destIsBack_, others_, enableHyperlinks;

/*
 *   Basic room exit lister.  This shows a list of the apparent exits from
 *   a location.
 *   
 *   The items to be listed are DestInfo objects.  
 */
class ExitLister: Lister
    showListPrefixWide(cnt, pov, parent)
    {
        if (cnt == 1)
            "The only obvious exit {leads|led} ";
        else
            "Obvious exits {lead|led} ";
    }
    showListSuffixWide(cnt, pov, parent) { ". "; }

    isListed(obj) { return true; }
    listCardinality(obj) { return 1; }

    showListItem(obj, options, pov, infoTab)
    {
        /*
         *   Show the back-to-direction prefix, if we don't know the
         *   destination name but this is the back-to direction: "back to
         *   the north" and so on. 
         */
        if (obj.destIsBack_ && obj.destName_ == nil)
            say(obj.dir_.backToPrefix + ' ');
        
        /* show the direction */
        showListItemDirName(obj, nil);

        /* if the destination is known, show it as well */
        if (obj.destName_ != nil)
        {
            /*
             *   if we have a list of other directions going to the same
             *   place, show it parenthetically 
             */
            otherExitLister.showListAll(obj.others_, 0, 0);
            
            /*
             *   Show our destination name.  If we know the "back to"
             *   destination name, show destination names in the format
             *   "east, to the living room" so that they are consistent
             *   with "west, back to the dining room".  Otherwise, just
             *   show "east to the living room".  
             */
            if ((options & hasBackNameFlag) != 0)
                ",";

            /* if this is the way back, say so */
            if (obj.destIsBack_)
                " back";

            /* show the destination */
            " to <<obj.destName_>>";
        }
    }
    showListSeparator(options, curItemNum, totalItems)
    {
        /*
         *   if we have a "back to" name, use the "long" notation - this is
         *   important because we'll use commas in the directions with
         *   known destination names 
         */
        if ((options & hasBackNameFlag) != 0)
            options |= ListLong;

        /*
         *   for a two-item list, if either item has a destination name,
         *   show a comma or semicolon (depending on 'long' vs 'short' list
         *   mode) before the "and"; for anything else, use the default
         *   handling 
         */
        if (curItemNum == 1
            && totalItems == 2
            && (options & hasDestNameFlag) != 0)
        {
            if ((options & ListLong) != 0)
                "; and ";
            else
                ", and ";
        }
        else
            inherited(options, curItemNum, totalItems);
    }

    /* show a direction name, hyperlinking it if appropriate */
    showListItemDirName(obj, initCap)
    {
        local dirname;
        
        /* get the name */
        dirname = obj.dir_.name;

        /* capitalize the first letter, if desired */
        if (initCap)
            dirname = dirname.substr(1,1).toUpper() + dirname.substr(2);

        /* show the name with a hyperlink or not, as configured */
        if (libGlobal.exitListerObj.enableHyperlinks)
            say(aHref(dirname, dirname));
        else
            say(dirname);
    }

    /* this lister shows destination names */
    listerShowsDest = true

    /*
     *   My special options flag: at least one object in the list has a
     *   destination name.  The caller should set this flag in our options
     *   if applicable. 
     */
    hasDestNameFlag = ListerCustomFlag(1)
    hasBackNameFlag = ListerCustomFlag(2)
    nextCustomFlag = ListerCustomFlag(3)
;

/*
 *   Show a list of other exits to a given destination.  We'll show the
 *   list parenthetically, if there's a list to show.  The objects to be
 *   listed are Direction objects.  
 */
otherExitLister: Lister
    showListPrefixWide(cnt, pov, parent) { " (or "; }
    showListSuffixWide(cnt, pov, parent) { ")"; }

    isListed(obj) { return true; }
    listCardinality(obj) { return 1; }

    showListItem(obj, options, pov, infoTab)
    {
        if (libGlobal.exitListerObj.enableHyperlinks)
            say(aHref(obj.name, obj.name));
        else
            say(obj.name);
    }
    showListSeparator(options, curItemNum, totalItems)
    {
        /*
         *   simply show "or" for all items (these lists are usually
         *   short, so omit any commas) 
         */
        if (curItemNum != totalItems)
            " or ";
    }
;

/*
 *   Show room exits as part of a room description, using the "verbose"
 *   sentence-style notation.  
 */
lookAroundExitLister: ExitLister
    showListPrefixWide(cnt, pov, parent)
    {
        /* add a paragraph break before the exit listing */
        "<.roompara>";

        /* inherit default handling */
        inherited(cnt, pov, parent);
    }    
;

/*
 *   Show room exits as part of a room description, using the "terse"
 *   notation. 
 */
lookAroundTerseExitLister: ExitLister
    showListPrefixWide(cnt, pov, parent)
    {
        "<.roompara><.parser>Obvious exits: ";
    }
    showListItem(obj, options, pov, infoTab)
    {
        /* show the direction name */
        showListItemDirName(obj, true);
    }
    showListSuffixWide(cnt, pov, parent)
    {
        "<./parser> ";
    }
    showListSeparator(options, curItemNum, totalItems)
    {
        /* just show a comma between items */
        if (curItemNum != totalItems)
            ", ";
    }

    /* this lister does not show destination names */
    listerShowsDest = nil
;

/*
 *   Show room exits in response to an explicit request (such as an EXITS
 *   command).  
 */
explicitExitLister: ExitLister
    showListEmpty(pov, parent)
    {
        "There {are|were} no obvious exits. ";
    }
;

/*
 *   Show room exits in the status line (used in HTML mode only)
 */
statuslineExitLister: ExitLister
    showListEmpty(pov, parent)
    {
        "<<statusHTML(3)>><b>Exits:</b> <i>None</i><<statusHTML(4)>>";
    }
    showListPrefixWide(cnt, pov, parent)
    {
        "<<statusHTML(3)>><b>Exits:</b> ";
    }
    showListSuffixWide(cnt, pov, parent)
    {
        "<<statusHTML(4)>>";
    }
    showListItem(obj, options, pov, infoTab)
    {
        "<<aHref(obj.dir_.name, obj.dir_.name, 'Go ' + obj.dir_.name,
                 AHREF_Plain)>>";
    }
    showListSeparator(options, curItemNum, totalItems)
    {
        /* just show a space between items */
        if (curItemNum != totalItems)
            " &nbsp; ";
    }

    /* this lister does not show destination names */
    listerShowsDest = nil
;

/*
 *   Implied action announcement grouper.  This takes a list of
 *   ImplicitActionAnnouncement reports and returns a single message string
 *   describing the entire list of actions.  
 */
implicitAnnouncementGrouper: object
    /*
     *   Configuration option: keep all failures in a list of implied
     *   announcements.  If this is true, then we'll write things like
     *   "trying to unlock the door and then open it"; if nil, we'll
     *   instead write simply "trying to unlock the door".
     *   
     *   By default, we keep only the first of a group of failures.  A
     *   group of failures is always recursively related, so the first
     *   announcement refers to the command that actually failed; the rest
     *   of the announcements are for the enclosing actions that triggered
     *   the first action.  All of the enclosing actions failed as well,
     *   but only because the first action failed.
     *   
     *   Announcing all of the actions is too verbose for most tastes,
     *   which is why we set the default here to nil.  The fact that the
     *   first action in the group failed means that we necessarily won't
     *   carry out any of the enclosing actions, so the enclosing
     *   announcements don't tell us much.  All they really tell us is why
     *   we're running the action that actually failed, but that's almost
     *   always obvious, so suppressing them is usually fine.  
     */
    keepAllFailures = nil

    /* build the composite message */
    compositeMessage(lst)
    {
        local txt;
        local ctx = new ListImpCtx();

        /* add the text for each item in the list */
        for (txt = '', local i = 1, local len = lst.length() ; i <= len ; ++i)
        {
            local curTxt;

            /* get this item */
            local cur = lst[i];

            /* we're not in a 'try' or 'ask' sublist yet */
            ctx.isInSublist = nil;

            /* set the underlying context according to this item */
            ctx.setBaseCtx(cur);

            /*
             *   Generate the announcement for this element.  Generate the
             *   announcement from the message property for this item using
             *   our running list context.  
             */
            curTxt = cur.getMessageText(
                cur.getAction().getOriginalAction(), ctx);

            /*
             *   If this one is an attempt only, and it's followed by one
             *   or more other attempts, the attempts must all be
             *   recursively related (in other words, the first attempt was
             *   an implied action required by the second attempt, which
             *   was required by the third, and so on).  They have to be
             *   recursively related, because otherwise we wouldn't have
             *   kept trying things after the first failed attempt.
             *   
             *   To make the series of failed attempts sound more natural,
             *   group them into a single "trying to", and keep only the
             *   first failure: rather than "trying to unlock the door,
             *   then trying to open the door", write "trying to unlock the
             *   door and then open it".
             *   
             *   An optional configuration setting makes us keep only the
             *   first failed operation, so we'd instead write simply
             *   "trying to unlock the door".
             *   
             *   Do the same grouping for attempts interrupted for an
             *   interactive question.  
             */
            while ((cur.justTrying && i < len && lst[i+1].justTrying)
                   || (cur.justAsking && i < len && lst[i+1].justAsking))
            {
                local addTxt;
                
                /*
                 *   move on to the next item - we're processing it here,
                 *   so we don't need to handle it again in the main loop 
                 */
                ++i;
                cur = lst[i];

                /* remember that we're in a try/ask sublist */
                ctx.isInSublist = true;

                /* add the list entry for this action, if desired */
                if (keepAllFailures)
                {
                    /* get the added text */
                    addTxt = cur.getMessageText(
                        cur.getAction().getOriginalAction(), ctx);

                    /*
                     *   if both the text so far and the added text are
                     *   non-empty, string them together with 'and then';
                     *   if one or the other is empty, use the non-nil one 
                     */
                    if (addTxt != '' && curTxt != '')
                        curTxt += ' and then ' + addTxt;
                    else if (addTxt != '')
                        curTxt = addTxt;
                }
            }

            /* add a separator before this item if it isn't the first */
            if (txt != '' && curTxt != '')
                txt += ', then ';

            /* add the current item's text */
            txt += curTxt;
        }

        /* if we ended up with no text, the announcement is silent */
        if (txt == '')
            return '';

        /* wrap the whole list in the usual full standard phrasing */
        return standardImpCtx.buildImplicitAnnouncement(txt);
    }
;

/*
 *   Suggested topic lister. 
 */
class SuggestedTopicLister: Lister
    construct(asker, askee, explicit)
    {
        /* remember the actors */
        askingActor = asker;
        targetActor = askee;

        /* remember whether this is explicit or implicit */
        isExplicit = explicit;

        /* cache the actor's scope list */
        scopeList = asker.scopeList();
    }
    
    showListPrefixWide(cnt, pov, parent)
    {
        /* add the asking and target actors as global message parameters */
        gMessageParams(askingActor, targetActor);

        /* show the prefix; include a paren if not in explicit mode */
        "<<isExplicit ? '' : '('>>{You askingActor/he} could ";
    }
    showListSuffixWide(cnt, pov, parent)
    {
        /* end the sentence; include a paren if not in explicit mode */
        ".<<isExplicit? '' : ')'>> ";
    }
    showListEmpty(pov, parent)
    {
        /*
         *   say that the list is empty if it was explicitly requested;
         *   say nothing if the list is being added by the library 
         */
        if (isExplicit)
        {
            gMessageParams(askingActor, targetActor);
            "<<isExplicit ? '' : '('>>{You askingActor/he} {have} nothing
            specific in mind {right now|just then} to discuss with
            {the targetActor/him}.<<isExplicit ? '' : ')'>> ";
        }
    }

    showListSeparator(options, curItemNum, totalItems)
    {
        /* use "or" as the conjunction */
        if (curItemNum + 1 == totalItems)
            ", or ";
        else
            inherited(options, curItemNum, totalItems);
    }

    /* list suggestions that are currently active */
    isListed(obj) { return obj.isSuggestionActive(askingActor, scopeList); }

    /* each item counts as one item grammatically */
    listCardinality(obj) { return 1; }

    /* suggestions have no contents */
    contentsListed(obj) { return nil; }

    /* get the list group */
    listWith(obj) { return obj.suggestionGroup; }

    /* mark as seen - nothing to do for suggestions */
    markAsSeen(obj, pov) { }

    /* show the item - show the suggestion's theName */
    showListItem(obj, options, pov, infoTab)
    {
        /* note that we're showing the suggestion */
        obj.noteSuggestion();

        /* show the name */
        say(obj.fullName);
    }

    /* don't use semicolons, even in long lists */
    longListSepTwo { listSepTwo; }
    longListSepMiddle { listSepMiddle; }
    longListSepEnd { listSepEnd; }

    /* flag: this is an explicit listing (i.e., a TOPICS command) */
    isExplicit = nil

    /* the actor who's asking for the topic list (usually the PC) */
    askingActor = nil

    /* the actor we're talking to */
    targetActor = nil

    /* our cached scope list for the actor */
    scopeList = nil
;

/* ASK/TELL suggestion list group base class */
class SuggestionListGroup: ListGroupPrefixSuffix
    showGroupItem(sublister, obj, options, pov, infoTab)
    {
        /*
         *   show the short name of the item - the group prefix will have
         *   shown the appropriate long name 
         */
        say(obj.name);
    }
;

/* ASK ABOUT suggestion list group */
suggestionAskGroup: SuggestionListGroup
    groupPrefix = "ask {it targetActor/him} about "
;

/* TELL ABOUT suggestion list group */
suggestionTellGroup: SuggestionListGroup
    groupPrefix = "tell {it targetActor/him} about "
;

/* ASK FOR suggestion list group */
suggestionAskForGroup: SuggestionListGroup
    groupPrefix = "ask {it targetActor/him} for "
;

/* GIVE TO suggestions list group */
suggestionGiveGroup: SuggestionListGroup
    groupPrefix = "give {it targetActor/him} "
;

/* SHOW TO suggestions */
suggestionShowGroup: SuggestionListGroup
    groupPrefix = "show {it targetActor/him} "
;

/* YES/NO suggestion group */
suggestionYesNoGroup: SuggestionListGroup
    showGroupList(pov, lister, lst, options, indent, infoTab)
    {
        /*
         *   if we have one each of YES and NO responses, make the entire
         *   list "say yes or no"; otherwise, use the default behavior 
         */
        if (lst.length() == 2
            && lst.indexWhich({x: x.ofKind(SuggestedYesTopic)}) != nil
            && lst.indexWhich({x: x.ofKind(SuggestedNoTopic)}) != nil)
        {
            /* we have a [yes, no] group - use the simple message */
            "say yes or no";
        }
        else
        {
            /* inherit the default behavior */
            inherited(pov, lister, lst, options, indent, infoTab);
        }
    }
    groupPrefix = "say";
;
