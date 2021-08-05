#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: command reports
 *   
 *   This module defines the "command report" classes, which the command
 *   execution engine uses to keep track of the status of a command.  
 */

#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Command report objects.  The library uses these to control how the
 *   text from a command is displayed.  Game code can also use report
 *   objects to show and control command results, but this isn't usually
 *   necessary; game code can usually simply display messages directly.
 *   
 *   Reports are divided into two broad classes: "default" and "full"
 *   reports.
 *   
 *   A "default" report is one that simply confirms that an action was
 *   performed, and provides little additional information.  The library
 *   uses default reports for simple commands whose full implications
 *   should normally be obvious to a player typing such commands: take,
 *   drop, put in, and the like.  The library's default reports are
 *   usually quite terse: "Taken", "Dropped", "Done".
 *   
 *   A "full" report is one that gives the player more information than a
 *   simple confirmation.  These reports typically describe either the
 *   changes to the game state caused by a command or surprising side
 *   effects of a command.  For example, if the command is "push button,"
 *   and pushing the button opens the door next to the button, a full
 *   report would describe the door opening.
 *   
 *   Note that a full report is warranted any time a command describes
 *   anything beyond a simple confirmation.  In our door-opening button
 *   example, opening the door by pushing the button always warrants a
 *   full report, even if the player has already seen the effects of the
 *   button a hundred times before, and even if the button is labeled
 *   "push to open door."  It doesn't matter whether or not the
 *   consequences of the command ought to be obvious to the player; what
 *   matters is that the command warrants a report beyond a simple
 *   confirmation.  Any time a report is more than a simple confirmation,
 *   it is a full report, no matter how obvious to the player the effects
 *   of the action.
 *   
 *   Full reports are further divided into three subcategories by time
 *   ordering: "main," "before," and "after."  "Before" and "after"
 *   reports are ordered before and after (respectively) a main report.  
 */
class CommandReport: object
    construct()
    {
        /* 
         *   remember the action with which we're associated, unless a
         *   subclass already specifically set the action 
         */
        if (action_ == nil)
            action_ = gAction;
    }

    /* get/set my action */
    getAction() { return action_; }
    setAction(action) { action_ = action; }

    /* check to see if my action is implicit */
    isActionImplicit() { return action_ != nil && action_.isImplicit; }

    /* check to see if my action is nested in the other report's action */
    isActionNestedIn(other)
    {
        return (action_ != nil
                && other.getAction() != nil
                && action_.isNestedIn(other.getAction()));
    }

    /*
     *   Flag: if this property is true, this report indicates a failure.
     *   By default, a report does not indicate failure.  
     */
    isFailure = nil

    /*
     *   Flag: if this property is true, this report indicates an
     *   interruption for interactive input. 
     */
    isQuestion = nil

    /* iteration number current when we were added to the transcript */
    iter_ = nil

    /* the action I'm associated with */
    action_ = nil

    /*
     *   Am I part of the same action as the given report?  Returns true if
     *   this action is part of the same iteration and part of the same
     *   action as the other report.  
     */
    isPartOf(report)
    {
        /* 
         *   if I don't have an action, or the other report doesn't have an
         *   action, we're not related 
         */
        if (action_ == nil || report.action_ == nil)
            return nil;

        /* if our iterations don't match, we're not related */
        if (iter_ != report.iter_)
            return nil;

        /* check if I'm part of the other report's action */
        return action_.isPartOf(report.action_);
    }
;

/*
 *   Group separator.  This simply displays separation between groups of
 *   messages - that is, between one set of messages associated with a
 *   single action and a set of messages associated with a different
 *   action.  
 */
class GroupSeparatorMessage: CommandReport
    construct(report)
    {
        /* use the same action and iteration as the given report */
        action_ = report.getAction();
        iter_ = report.iter_;
    }

    /* show the normal command results separator */
    showMessage() { say(gLibMessages.complexResultsSeparator); }
;

/*
 *   Internal separator.  This displays separation within a group of
 *   messages for a command, to visually separate the results from an
 *   implied command from the results for the enclosing command. 
 */
class InternalSeparatorMessage: CommandReport
    construct(report)
    {
        /* use the same action and iteration as the given report */
        action_ = report.getAction();
        iter_ = report.iter_;
    }

    /* show the normal command results separator */
    showMessage() { say(gLibMessages.internalResultsSeparator); }
;

/*
 *   Report boundary marker.  This is a pseudo-report that doesn't display
 *   anything; its purpose is to allow a caller to identify a block of
 *   reports (the reports between two markers) for later removal or
 *   reordering.  
 */
class MarkerReport: CommandReport
    showMessage() { }
;

/*
 *   End-of-description marker.  This serves as a marker in the transcript
 *   stream to let us know where the descriptive reports for a given
 *   action end.  
 */
class EndOfDescReport: MarkerReport
;

/*
 *   Simple MessageResult-based command report 
 */
class CommandReportMessage: CommandReport, MessageResult
    construct([params])
    {
        /* invoke our base class constructors */
        inherited CommandReport();
        inherited MessageResult(params...);
    }
;

/* 
 *   default report 
 */
class DefaultCommandReport: CommandReportMessage
;

/*
 *   extra information report 
 */
class ExtraCommandReport: CommandReportMessage
;

/*
 *   default descriptive report 
 */
class DefaultDescCommandReport: CommandReportMessage
;

/*
 *   cosmetic spacing report 
 */
class CosmeticSpacingCommandReport: CommandReportMessage
;

/*
 *   base class for all "full" reports 
 */
class FullCommandReport: CommandReportMessage
    /* 
     *   a full report has a sequence number that tells us where the
     *   report goes relative to the others - the higher this number, the
     *   later the report goes 
     */
    seqNum = nil
;

/*
 *   "before" report - these come before the main report 
 */
class BeforeCommandReport: FullCommandReport
    seqNum = 1
;

/*
 *   main report 
 */
class MainCommandReport: FullCommandReport
    seqNum = 2
;

/*
 *   failure report 
 */
class FailCommandReport: FullCommandReport
    seqNum = 2
    isFailure = true
;

/*
 *   failure marker - this is a silent report that marks an action as
 *   having failed without actually generating any message text 
 */
class FailCommandMarker: MarkerReport
    isFailure = true
;

/*
 *   "after" report - these come after the main report 
 */
class AfterCommandReport: FullCommandReport
    seqNum = 3
;

/*
 *   An interruption for interactive input.  This is used to report a
 *   prompt for more information that's needed before the command can
 *   proceed, such as a prompt for a missing object, or a disambiguation
 *   prompt. 
 */
class QuestionCommandReport: MainCommandReport
    isQuestion = true;
;

/*
 *   A conversation begin/end report.  This is a special marker we insert
 *   into the transcript to flag the boundaries of an NPC's conversational
 *   message.  
 */
class ConvBoundaryReport: CommandReport
    construct(id) { actorID = id; }

    /* the actor's ID number, as assigned by the ConversationManager */
    actorID = nil
;
class ConvBeginReport: ConvBoundaryReport
    showMessage() { say('<.convbegin ' + actorID + '>'); }
;
class ConvEndReport: ConvBoundaryReport
    construct(id, node)
    {
        inherited(id);
        defConvNode = node;
    }
    
    showMessage()
    {
        if (actorID != nil)
            say('<.convend ' + actorID + ' '
                + (defConvNode == nil ? '' : defConvNode) + '>');
    }

    /* the default new ConvNode for the actor */
    defConvNode = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Announcements.  We use these to track announcements to be made as
 *   part of an action's results. 
 */
class CommandAnnouncement: CommandReport
    construct([params])
    {
        /* inherit default handling */
        inherited();

        /* remember our text */
        messageText_ = getMessageText(params...);
    }

    /* 
     *   Get our message text.  By default, we simply get the gLibMessages
     *   message given by the property. 
     */
    getMessageText([params])
    {
        /* get the library message */
        return gLibMessages.(messageProp_)(params...);
    }

    /* 
     *   Show our message.  Our default implementation shows the library
     *   message given by our messageProp_ property, using the parameters
     *   we stored in our constructor.  
     */
    showMessage()
    {
        /* call gLibMessages to show our message */
        say(messageText_);
    }

    /* our gLibMessages property */
    messageProp_ = nil

    /* our message text */
    messageText_ = ''
;

/*
 *   Multiple-object announcement.  When the player applies a single
 *   command to a series of objects (as in "take the book and the folder"
 *   or "take all"), we'll show one of these announcements for each object,
 *   just before we execute the command for that object.  This announcement
 *   usually just shows the object's name plus suitable punctuation (in
 *   English, a colon), and helps the player see which results go with
 *   which objects.  
 */
class MultiObjectAnnouncement: CommandAnnouncement
    construct(preCalcMsg, obj, whichObj, action)
    {
        /* do the inherited work */
        inherited(obj, whichObj, action);

        /* 
         *   if we have a pre-calculated message, use it instead of the
         *   message we just generated - this lets the caller explicitly
         *   set the message as desired 
         */
        if (preCalcMsg != nil)
            messageText_ = preCalcMsg;
    }

    /* show the announceMultiActionObject message */
    messageProp_ = &announceMultiActionObject
;

/*
 *   Default object announcement.  We display this announcement whenever
 *   the player leaves out a required object from a command, but the parser
 *   is able to infer which object they must have meant.  The parser infers
 *   that an object was intended when a verb requires an object that the
 *   player didn't specify, and there's only one logical choice for the
 *   missing object.  We announce our assumption to put it out in the open,
 *   to ensure that the player is immediately alerted if they had something
 *   else in mind.
 *   
 *   In English, this type of announcement conventionally consists of
 *   simply the name of the assumed object, in parenthesis and on a line by
 *   itself.  In cases where the object role involves a prepositional
 *   phrase in the verb structure, we generally show the preposition before
 *   the object name.  This format usually reads intuitively, by combining
 *   with the text just above of the player's own command:
 *
 *.  >open
 *.  (the door>
 *.  You try opening the door, but it seems to be locked.
 *.
 *.  >unlock the door
 *   (with the key)
 */
class DefaultObjectAnnouncement: CommandAnnouncement
    construct(obj, whichObj, action, allResolved)
    {
        /* remember our object */
        obj_ = obj;

        /* remember the message parameters */
        whichObj_ = whichObj;
        allResolved_ = allResolved;

        /* remember my action */
        action_ = action;

        /* inherit default handling */
        inherited();
    }

    /* get our message text */
    getMessageText()
    {
        /* get the announcement message from our object */
        return obj_.announceDefaultObject(whichObj_, action_, allResolved_);
    }

    /* our defaulted object */
    obj_ = nil

    /* our message parameters */
    whichObj_ = nil
    allResolved_ = nil
;

/*
 *   Ambiguous object announcement.  We display this when the parser
 *   manages to resolve a noun phrase to an object (or objects) from an
 *   ambiguous set of possibilities, without having to ask the player for
 *   help but also without absolute certainty that the objects selected are
 *   the ones the player meant.  This happens when more than enough objects
 *   are logical possibilities for selection, but some objects are more
 *   logical choices than others.  The parser picks the most logical of the
 *   available options, but since other logical choices are present, the
 *   parser can't be certain that it chose the ones the player actually
 *   meant.  Because of this uncertainty, we generate one of these
 *   announcements each time this happens.  This report lets the player
 *   know exactly which object we chose, which will immediately alert the
 *   player when our selection is different from what they had in mind.
 *   
 *   In form, this type of announcement usually looks just like a default
 *   object announcement.  
 */
class AmbigObjectAnnouncement: CommandAnnouncement
    /* show the announceAmbigObject announcement */
    messageProp_ = &announceAmbigActionObject
;

/*
 *   Remapped action announcement.  This is used when we need to mention a
 *   defaulted or disambiguated object, but the player's original input was
 *   remapped to a different action that rearranges the object roles.  In
 *   these cases, rather than just announcing the defaulted object name, we
 *   announce the entire remapped action; we show the full action
 *   description because rearrangement of the object roles usually makes
 *   the standard object-only announcement confusing to read, since it
 *   doesn't naturally fit in as a continuation of what the user typed.
 *   
 *   In English, this message is usually shown with the entire verb phrase,
 *   in present participle form ("opening the door"), enclosed in
 *   parentheses and on a line by itself.  
 */
class RemappedActionAnnouncement: CommandAnnouncement
    construct()
    {
        /* use the action as the message parameter */
        inherited(gAction);
    }

    messageProp_ = &announceRemappedAction
;

/*
 *   Each language module must define a class called
 *   ImplicitAnnouncementContext, and three instances of the class, for use
 *   by the generic library.  The language module can define other
 *   instances of the context class as needed.  We minimally need the
 *   following instances to be defined by the language module:
 *   
 *   standardImpCtx: this is the standard context, which indicates that we
 *   want the default format for the implicit action announcement.
 *   
 *   tryingImpCtx: this is the "trying" context, which indicates that we
 *   want the announcement to phrase the action to indicate that we're only
 *   trying the action, not actually performing it.  We use this when the
 *   implicit action has failed, in which case we want our announcement to
 *   say that we're merely attempting the action; the announcement
 *   shouldn't imply that the action has actually been performed.
 *   
 *   askingImpCtx: this is the "asking" context, which indicates that the
 *   action was interrupted with an interactive question, such as a prompt
 *   for a missing direct object.
 *   
 *   We leave it up to the language module to define the class and these
 *   two instances.  This lets the language module represent the context
 *   types any way it likes.  
 */

/*
 *   Implicit action announcement.  This is displayed when we perform a
 *   command implicitly, which we usually do to fulfill a precondition of
 *   an action.
 *   
 *   In English, we usually show an implied action as the verb participle
 *   phrase ("opening the door"), prefixed with "first", and enclosed in
 *   parentheses on a line by itself (hence, "(first opening the door)").  
 */
class ImplicitActionAnnouncement: CommandAnnouncement
    construct(action, msg)
    {
        /* use the given message property */
        messageProp_ = msg;

        /* 
         *   Inherit default.  The first message parameter is the action;
         *   the second is our standard implicit action context object,
         *   indicating that we want the normal context.  
         */
        inherited(action, standardImpCtx);
    }

    /* 
     *   Make this announcement silent.  This eliminates any announcement
     *   for this action, but makes it otherwise behave like a normal
     *   implied action. 
     */
    makeSilent()
    {
        /* clear my message text */
        messageText_ = '';

        /* 
         *   use the silent announcement message if we have to regenerate
         *   our text for another context 
         */
        messageProp_ = &silentImplicitAction;
    }

    /*
     *   Note that the action we're attempting is merely an attempt that
     *   failed.  This will change our report to indicate that we're only
     *   trying the action, rather than suggesting that we actually carried
     *   it out.  
     */
    noteJustTrying()
    {
        /* note that we're just trying the action */
        justTrying = true;

        /* change our message to the "trying" form */
        messageText_ = getMessageText(getAction(), tryingImpCtx);
    }

    /* 
     *   Note that the action we're attempting is incomplete, as it was
     *   interupted for interactive input (such as asking for a missing
     *   object). 
     */
    noteQuestion()
    {
        /* note that the action was interrupted with a question */
        justAsking = true;

        /* change our message to the "asking" form */
        messageText_ = getMessageText(getAction(), askingImpCtx);
    }

    /* 
     *   Flag: we're just attempting the action; this is set when we
     *   determine that the implicit action has failed, in which case we
     *   want an announcement indicating that we're merely attempting the
     *   action, not actually performing it.  Presume that we're actually
     *   going to perform the action; the action can change this if
     *   necessary.  
     */
    justTrying = nil

    /* flag: the action was interrupted with an interactive question */
    justAsking = nil
;

class CommandSepAnnouncement: CommandAnnouncement
    construct()
    {
        /* we're not associated with an iteration or action */
        action_ = nil;
        iter_ = 0;
    }

    showMessage()
    {
        /* show a command separator */
        "<.commandsep>";
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Command Transcript.  This is a "semantic transcript" of the results of
 *   a command.  This provides a list of CommandReport objects describing
 *   the results of the command.  
 */
class CommandTranscript: OutputFilter
    construct()
    {
        /* set up a vector to hold the reports */
        reports_ = new Vector(5);
    }

    /* 
     *   flag: the command has failed (i.e., at least one failure report
     *   has been generated) 
     */
    isFailure = nil

    /*
     *   Note that the current action has failed.  This is equivalent to
     *   adding a reportFailure() message to the transcript.  
     */
    noteFailure()
    {
        /* add an empty reportFailure message */
        reportFailure('');
    }

    /*
     *   Did the given action fail?  This scans the transcript to determine
     *   if there are any failure messages associated with the given
     *   action.   
     */
    actionFailed(action)
    {
        /* 
         *   scan the transcript for failure messages that are associated
         *   with the given action 
         */
        return reports_.indexWhich(
            {x: x.isPartOf(action) && x.isFailure}) != nil;
    }

    /* 
     *   flag: I'm active; when this is nil, we'll pass text through our
     *   filter routine unchanged 
     */
    isActive = true

    /*
     *   Summarize the current action's reports.  This allows a caller to
     *   turn a series of iterated reports into a single report for the
     *   entire action.  For example, we could change something like this:
     *   
     *   gold coin: Bob accepts the gold coin.
     *.  gold coin: Bob accepts the gold coin.
     *.  gold coin: Bob accepts the gold coin.
     *   
     *   into this:
     *   
     *   Bob accepts the three gold coins.
     *   
     *   This function runs through the reports for the current action,
     *   submitting each one to the 'cond' callback to see if it's of
     *   interest to the summary.  For each consecutive run of two or more
     *   reports that can be summarized, we'll remove the reports that
     *   'cond' accepted, and we'll remove the multiple-object announcement
     *   reports associated with them, and we'll insert a new report with
     *   the message returned by the 'report' callback.
     *   
     *   'cond' is called as cond(x), where 'x' is a report object.  This
     *   callback returns true if the report can be summarized for the
     *   caller's purposes, nil if not.
     *   
     *   'report' is called as report(vec), where 'vec' is a Vector
     *   consisting of all of the consecutive report objects that we're now
     *   summarizing.  This function returns a string giving the message to
     *   use in place of the reports we're removing.  This should be a
     *   summary message, standing in for the set of individual reports
     *   we're removing.
     *   
     *   There's an important subtlety to note.  If the messages you're
     *   summarizing are conversational (that is, if they're generated by
     *   TopicEntry responses), you should take care to generate the full
     *   replacement text in the 'report' part, rather than doing so in
     *   separate code that you run after summarizeAction() returns.  This
     *   is important because it ensures that the Conversation Manager
     *   knows that your replacement message is part of the same
     *   conversation.  If you wait until after summarizeAction() returns
     *   to generate more response text, the conversation manager won't
     *   realize that the additional text is part of the same conversation.
     */
    summarizeAction(cond, report)
    {
        local vec = new Vector(8);
        local rpt = reports_;
        local cnt = rpt.length();
        local i;

        /* find the first report for the current action */
        for (i = 1 ; i <= cnt && rpt[i].getAction() != gAction ; ++i) ;

        /* iterate over the transcript for the current action */
        for ( ; ; ++i)
        {
            local ok;

            /* presume we won't find a summarizable item */
            ok = nil;
            
            /* if we're still in range, check what we have */
            if (i <= cnt)
            {
                /* get the current item */
                local cur = rpt[i];

                /* if this one is of interest, note it in the vector */
                if (cond(cur))
                {
                    /* add it to our vector of summarizable reports */
                    vec.append(cur);

                    /* note that we're okay on this round */
                    ok = true;
                }
                else if (cur.ofKind(ImplicitActionAnnouncement)
                         || cur.ofKind(MultiObjectAnnouncement)
                         || cur.ofKind(DefaultCommandReport)
                         || cur.ofKind(ConvBoundaryReport))
                {
                    /* we can keep these in summaries */
                    ok = true;
                }
            }

            /* 
             *   If this item isn't summarizable, or we've reached the last
             *   item, generate a summary of any we have so far.  (We need
             *   to generate the summary on reaching the last item because
             *   there are no further items that could go in the summary.)
             */
            if (!ok || i == cnt)
            {
                /* if we have two or more items, generate a summary */
                if (vec.length() > 1)
                {
                    local insIdx;
                    local txt;
                
                    /* remove each item in the vector */
                    foreach (local cur in vec)
                    {
                        local idx;
                        
                        /* get the index of the current item */
                        idx = rpt.indexOf(cur);

                        /* 
                         *   we're summarizing this item, so remove the
                         *   individual item - subsume it into the summary 
                         */
                        rpt.removeElementAt(idx);
                        --i;
                        --cnt;

                        /* insert the summary here */
                        insIdx = idx;

                        /* 
                         *   skip any implicit action announcements,
                         *   default command announcements, and
                         *   conversational boundary markers 
                         */
                        for (--idx ;
                             idx > 0
                             && (rpt[idx].ofKind(ImplicitActionAnnouncement)
                                 || rpt[idx].ofKind(DefaultCommandReport)
                                 || rpt[idx].ofKind(ConvBoundaryReport)) ;
                             --idx) ;

                        /*
                         *   if the preceding element is a multi-object
                         *   announcement, remove it - let the summary
                         *   mention the individual objects if it wants to 
                         */
                        if (idx > 0
                            && rpt[idx].ofKind(MultiObjectAnnouncement))
                        {
                            /* remove this element and adjust the counters */
                            rpt.removeElementAt(idx);
                            --i;
                            --cnt;
                            --insIdx;

                            /*
                             *   If this leaves us with a <.convbegin>
                             *   preceded directly by a <.convend> for the
                             *   same actor, the two cancel each other out.
                             *   Simply remove both of them.  
                             */
                            if (idx <= rpt.length()
                                && idx > 1
                                && rpt[idx].ofKind(ConvBeginReport)
                                && rpt[idx-1].ofKind(ConvEndReport)
                                && rpt[idx].actorID == rpt[idx-1].actorID)
                            {
                                /* 
                                 *   we have a canceling <.convend> +
                                 *   <.convbegin> pair - simply remove them
                                 *   both and adjust the counters
                                 *   accordingly 
                                 */
                                rpt.removeRange(idx - 1, idx);
                                i -= 2;
                                cnt -= 2;
                                insIdx -= 2;
                            }
                        }
                    }

                    /* ask the caller for the summary */
                    txt = report(vec);

                    /* insert it */
                    rpt.insertAt(insIdx, new MainCommandReport(txt));
                    ++cnt;
                    ++i;
                }

                /* clear the vector */
                if (vec.length() > 0)
                    vec.removeRange(1, vec.length());
            }

            /* if we've reached the end of the list, we're done */
            if (i > cnt)
                break;
        }
    }

    /* activate - set up to capture output */
    activate()
    {
        /* make myself active */
        isActive = true;
    }

    /* deactivate - stop capturing output */
    deactivate()
    {
        /* make myself inactive */
        isActive = nil;
    }

    /* 
     *   Count an iteration.  An Action should call this once per iteration
     *   if it's a top-level (non-nested) command.
     */
    newIter() { ++iter_; }

    /* 
     *   Flush the transcript in preparation for reading input.  This shows
     *   all pending reports, clears the backlog of reports (so that we
     *   don't show them again in the future), and deactivates the
     *   transcript's capture feature so that subsequent output goes
     *   directly to the output stream.
     *   
     *   We return the former activation status - that is, we return true
     *   if the transcript was activated before the call, nil if not.  
     */
    flushForInput()
    {
        /* show our reports, and deactivate output capture */
        local wasActive = showReports(true);

        /* clear the reports, since we've now shown them all */
        clearReports();

        /* return the previous activation status */
        return wasActive;
    }

    /* 
     *   Show our reports.  Returns true if the transcript was previously
     *   active, nil if not. 
     */
    showReports(deact)
    {
        local wasActive;
        
        /* 
         *   remember whether we were active or not originally, then
         *   deactivate (maybe just temporarily) so that we can write out
         *   our reports without recursively intercepting them
         */
        wasActive = isActive;
        deactivate();

        /* first, apply all defined transformations to our transcript */
        applyTransforms();
        
        /*
         *   Temporarily cancel any sense context message blocking.  We
         *   have already taken into account for each report whether or
         *   not the report was visible when it was generated, so we can
         *   display each report that made it past that check without any
         *   further conditions.  
         */
        callWithSenseContext(nil, nil, function()
        {
            /* show the reports */            
            foreach (local cur in reports_)
            {
                /* if we're allowed to show this report, show it */
                if (canShowReport(cur))
                    cur.showMessage();
            }
        });
            
        /* 
         *   if we were active and we're not being asked to deactivate,
         *   re-activate now that we're finished showing our reports 
         */
        if (wasActive && !deact)
            activate();

        /* return the former activation status */
        return wasActive;
    }

    /*
     *   Add a report. 
     */
    addReport(report)
    {
        /* check for a failure report */
        if (report.isFailure)
        {
            /* set the failure flag for the entire command */
            isFailure = true;

            /* 
             *   If this is an implied command, and the actor is in "NPC
             *   mode", suppress the report.  When an implied action fails
             *   in NPC mode, we act as though we never attempted the
             *   action. 
             */
            if (gAction.isImplicit && gActor.impliedCommandMode() == ModeNPC)
            {
                /* add a failure marker, not the message report */
                reports_.append(new FailCommandMarker());

                /* that's all we need to add */
                return;
            }
        }

        /* 
         *   Do not queue reports made while the sense context is blocking
         *   output because the player character cannot sense the locus of
         *   the action.  Note that this check comes before we queue the
         *   report, but after we've noted any effect on the status of the
         *   overall action; even if we're not going to show the report,
         *   its status effects are still valid.  
         */
        if (senseContext.isBlocking)
            return;

        /* 
         *   If the new report's iteration ID hasn't been set already, note
         *   the current iteration in the report.  Some types of reports
         *   will have already set a specific iteration before we get here,
         *   so set the iteration ID only if the report hasn't done so
         *   already.  
         */
        if (report.iter_ == nil)
            report.iter_ = iter_;

        /* append the report */
        reports_.append(report);
    }

    /* get the last report added */
    getLastReport()
    {
        local cnt = reports_.length();
        return (cnt == 0 ? nil : reports_[cnt]);
    }

    /* delete the last report added */
    deleteLastReport()
    {
        local cnt = reports_.length();
        if (cnt != 0)
            reports_.removeElementAt(cnt);
    }

    /*
     *   Add a marker report.  This adds a marker to the report stream,
     *   and returns the marker object.  The marker doesn't show any
     *   message in the final display, but callers can use a pair of
     *   markers to identify a range of reports for later reordering or
     *   removal. 
     */
    addMarker()
    {
        /* create the new report */
        local marker = new MarkerReport();

        /* add it to the stream */
        addReport(marker);

        /* return the new report */
        return marker;
    }

    /* delete the reports between two markers */
    deleteRange(marker1, marker2)
    {
        local idx1, idx2;
        
        /* find the indices of the two markers */
        idx1 = reports_.indexOf(marker1);
        idx2 = reports_.indexOf(marker2);

        /* if we found both, delete the range */
        if (idx1 != nil && idx2 != nil)
            reports_.removeRange(idx1, idx2);
    }

    /* 
     *   Pull out the reports between two markers, and reinsert them at
     *   the end of the transcript.  
     */
    moveRangeAppend(marker1, marker2)
    {
        local idx1, idx2;
        
        /* find the indices of the two markers */
        idx1 = reports_.indexOf(marker1);
        idx2 = reports_.indexOf(marker2);

        /* if we didn't find both, ignore the request */
        if (idx1 == nil || idx2 == nil)
            return;

        /* append each item in the range to the end of the report list */
        for (local i = idx1 ; i <= idx2 ; ++i)
            reports_.append(reports_[i]);

        /* delete the original copies */
        reports_.removeRange(idx1, idx2);
    }

    /* 
     *   Perform a callback on all of the reports in the transcript.
     *   We'll invoke the given callback function func(rpt) once for each
     *   report, with the report object as the parameter. 
     */
    forEachReport(func) { reports_.forEach(func); }

    /*
     *   End the description section of the report.  This adds a marker
     *   report that indicates that anything following (and part of the
     *   same action) is no longer part of the description; this can be
     *   important when we apply the default description suppression
     *   transformation, because it tells us not to consider the
     *   non-descriptive messages following this marker when, for example,
     *   suppressing default descriptive messages.  
     */
    endDescription()
    {
        /* add an end-of-description report */
        addReport(new EndOfDescReport());
    }

    /*
     *   Announce that the action is implicit
     */
    announceImplicit(action, msgProp)
    {
        /* 
         *   If the actor performing the command is not in "player" mode,
         *   save an implicit action announcement; for NPC mode, we treat
         *   implicit command results like any other results, so we don't
         *   want a separate announcement.  
         */
        if (gActor.impliedCommandMode() == ModePlayer)
        {
            /* create the new report */
            local report = new ImplicitActionAnnouncement(action, msgProp);

            /* add it to the transcript */
            addReport(report);

            /* return it */
            return report;
        }
        else
        {
            /* no need for a report */
            return nil;
        }
    }

    /*
     *   Announce a remapped action 
     */
    announceRemappedAction()
    {
        /* save a remapped-action announcement */
        addReport(new RemappedActionAnnouncement());
    }

    /*
     *   Announce one of a set of objects to a multi-object action.  We'll
     *   record this announcement for display with our report list.  
     */
    announceMultiActionObject(preCalcMsg, obj, whichObj)
    {
        /* save a multi-action object announcement */
        addReport(new MultiObjectAnnouncement(
            preCalcMsg, obj, whichObj, gAction));
    }

    /*
     *   Announce an object that was resolved with slight ambiguity. 
     */
    announceAmbigActionObject(obj, whichObj)
    {
        /* save an ambiguous object announcement */
        addReport(new AmbigObjectAnnouncement(obj, whichObj, gAction));
    }

    /*
     *   Announce a default object. 
     */
    announceDefaultObject(obj, whichObj, action, allResolved)
    {
        /* save the default object announcement */
        addReport(new DefaultObjectAnnouncement(
            obj, whichObj, action, allResolved));
    }

    /*
     *   Add a command separator. 
     */
    addCommandSep()
    {
        /* add a command separator announcement */
        addReport(new CommandSepAnnouncement());
    }

    /*
     *   clear our reports 
     */
    clearReports()
    {
        /* forget all of the reports in the main list */
        if (reports_.length() != 0)
            reports_.removeRange(1, reports_.length());
    }

    /*
     *   Can we show a given report?  By default, we always return true,
     *   but subclasses might want to override this to suppress certain
     *   types of reports.  
     */
    canShowReport(report) { return true; }

    /*
     *   Filter text.  If we're active, we'll turn the text into a command
     *   report and add it to our report list, blocking the text from
     *   reaching the underlying stream; otherwise, we'll pass it through
     *   unchanged.  
     */
    filterText(ostr, txt)
    {
        /* if we're inactive, pass text through unchanged */
        if (!isActive)
            return txt;
        
        /* 
         *   If the current sense context doesn't allow any messages to be
         *   generated, block the generated text entirely.  We want to
         *   block text or not according to the sense context in effect
         *   now; so we must note it now rather than wait until we
         *   actually display the report, since the context could be
         *   different by then.  
         */
        if (senseContext.isBlocking)
            return nil;

        /* add a main report to our list if the text is non-empty */
        if (txt != '')
            addReport(new MainCommandReport(txt));

        /* capture the text - send nothing to the underlying stream */
        return nil;
    }

    /* apply transformations */
    applyTransforms()
    {
        /* apply each defined transformation */
        foreach (local cur in transforms_)
            cur.applyTransform(self, reports_);
    }

    /* 
     *   check to see if the current action has a report matching the given
     *   criteria 
     */
    currentActionHasReport(func)
    {
        /* check to see if we can find a matching report */
        return (findCurrentActionReport(func) != nil);
    }

    /* find a report in the current action that matches the given criteria */
    findCurrentActionReport(func)
    {
        /* 
         *   Find an action that's part of the current iteration and which
         *   matches the given function's criteria.  Return the first match
         *   we find.  
         */
        return reports_.valWhich({x: x.iter_ == iter_ && (func)(x)});
    }

    /* 
     *   iteration number - for an iterated top-level command, this helps
     *   us keep the results for a particular iteration grouped together 
     */
    iter_ = 1

    /* our vector of reports */
    reports_ = nil

    /* our list of transformations */
    transforms_ = [defaultReportTransform, implicitGroupTransform,
                   reportOrderTransform, complexMultiTransform]
;

/* ------------------------------------------------------------------------ */
/*
 *   Transcript Transform. 
 */
class TranscriptTransform: object
    /*
     *   Apply our transform to the transcript vector.  By default, we do
     *   nothing; each subclass must override this to manipulate the vector
     *   to make the change it wants to make.  
     */
    applyTransform(trans, vec) { }
;

/* ------------------------------------------------------------------------ */
/*
 *   Transcript Transform: set before/main/after report order.  We'll look
 *   for any before/after reports that are out of order with respect to
 *   their main reports, and move them into the appropriate positions. 
 */
reportOrderTransform: TranscriptTransform
    applyTransform(trans, vec)
    {
        /* scan for before/after reports */
        for (local i = 1, local len = vec.length() ; i <= len ; ++i)
        {
            /* get this item */
            local cur = vec[i];

            /* if this is a before/after report, consider moving it */
            if (cur.ofKind(FullCommandReport) && cur.seqNum != nil)
            {
                local idx;
                
                /* 
                 *   This item cares about its sequencing, so it could be
                 *   out of order with respect to other items from the same
                 *   sequence.  Find the first item with a higher sequence
                 *   number from the same group, and make sure this item is
                 *   before the first such item.  
                 */
                for (idx = 1 ; idx < i ; ++idx)
                {
                    local x;
                    
                    /* get this item */
                    x = vec[idx];

                    /* if x should come after cur, we need to move cur */
                    if (x.ofKind(FullCommandReport)
                        && x.seqNum > cur.seqNum
                        && x.isPartOf(cur))
                    {
                        /* remove cur and reinsert it before x */
                        vec.removeElementAt(i);
                        vec.insertAt(idx, cur);

                        /* adjust our scan index for the removal */
                        --i;
                        
                        /* no need to look any further */
                        break;
                    }
                }
            }
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Transcript Transform: remove unnecessary default reports.  We'll scan
 *   the transcript for default reports for actions which also have
 *   implicit announcements or non-default reports, and remove those
 *   default reports.  We'll also remove default descriptive reports which
 *   also have non-default reports in the same action.  
 */
defaultReportTransform: TranscriptTransform
    applyTransform(trans, vec)
    {
        /* scan for default reports */
        for (local i = 1, local len = vec.length() ; i <= len ; ++i)
        {
            local cur;
            
            /* get this item */
            cur = vec[i];
            
            /* 
             *   if this is a default report, check to see if we want to
             *   keep it 
             */
            if (cur.ofKind(DefaultCommandReport))
            {
                /* 
                 *   check for a main report or an implicit announcement
                 *   associated with the same action; if we find anything,
                 *   we don't need to keep the default report 
                 */
                if (vec.indexWhich(
                    {x: (x != cur
                         && cur.isPartOf(x)
                         && (x.ofKind(FullCommandReport)
                             || x.ofKind(ImplicitActionAnnouncement)))
                    }) != nil)
                {
                    /* we don't need this default report */
                    vec.removeElementAt(i);

                    /* adjust our scan index for the removal */
                    --i;
                    --len;
                }
            }

            /*
             *   if this is a default descriptive report, check to see if
             *   we want to keep it 
             */
            if (cur.ofKind(DefaultDescCommandReport))
            {
                local fullIdx;
                
                /* 
                 *   check for a main report associated with the same
                 *   action
                 */
                fullIdx = vec.indexWhich(
                    {x: (x != cur
                         && cur.isPartOf(x)
                         && x.ofKind(FullCommandReport))});
                
                /* 
                 *   if we found another report, check to see if it comes
                 *   before or after any 'end of description' for the same
                 *   action 
                 */
                if (fullIdx != nil)
                {
                    local endIdx;

                    /* find the 'end of description' report, if any */
                    endIdx = vec.indexWhich(
                        {x: (x != cur
                             && cur.isPartOf(x)
                             && x.ofKind(EndOfDescReport))});

                    /* 
                     *   if we found a full report before the
                     *   end-of-description report, then the full report is
                     *   part of the description and thus should suppress
                     *   the default report; otherwise, the description
                     *   portion includes only the default report and the
                     *   default report should thus remain 
                     */
                    if (endIdx == nil || fullIdx < endIdx)
                    {
                        /* don't keep the default descriptive report */
                        vec.removeElementAt(i);

                        /* adjust our indices for the removal */
                        --i;
                        --len;
                    }
                }
            }
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Transcript Transform: group implicit announcements.  We'll find any
 *   runs of consecutive implicit command announcements, and group each run
 *   into a single announcement listing all of the implied actions.  For
 *   example, we'll turn this:
 *   
 *.  >go south
 *.  (first opening the door)
 *.  (first unlocking the door)
 *   
 *   this into:
 *   
 *.  >go south
 *.  (first opening the door and unlocking the door)
 *   
 *   In addition, if we find an implicit announcement in the middle of a
 *   set of regular command reports, and it's for an action nested within
 *   the action generating the regular reports, we'll start a new paragraph
 *   before the implicit announcement.  
 */
implicitGroupTransform: TranscriptTransform
    applyTransform(trans, vec)
    {
        /* 
         *   Scan for implicit announcements whose actions failed, and mark
         *   the implicit actions as such.  This allows us to phrase the
         *   implicit announcements as attempts rather than as actual
         *   actions, which sounds a little better because it doesn't clash
         *   with the failure report that immediately follows.  
         */
        for (local i = 1, local len = vec.length() ; i <= len ; ++i)
        {
            local sub;
            
            /* get this item */
            local cur = vec[i];

            /* 
             *   If this is an implicit action announcement, and its
             *   corresponding action (or any nested action) failed, mark
             *   the implicit announcement as a mere attempt.  Likewise, if
             *   we're interrupting the action for interactive input, it's
             *   likewise just an incomplete attempt.  
             */
            if (cur.ofKind(ImplicitActionAnnouncement)
                && (sub = vec.valWhich(
                    {x: ((x.isFailure || x.isQuestion)
                         && (x.isPartOf(cur)
                             || x.isActionNestedIn(cur)))})) != nil)
            {
                /* 
                 *   it's either a failed attempt or an interruption for a
                 *   question - note which one 
                 */
                if (sub.isFailure)
                    cur.noteJustTrying();
                else
                    cur.noteQuestion();
            }
        }

        /* 
         *   Scan for implicit announcement groups.  Since we're only
         *   scanning for runs of two or more announcements, we can stop
         *   scanning one short of the end of the list - there's no need to
         *   check the last item because it can't possibly be followed by
         *   another item.  Thus, scan while i < len.  
         */
        for (local i = 1, local len = vec.length() ; i < len ; ++i)
        {
            local origI = i;
            
            /* get this item */
            local cur = vec[i];
            
            /* 
             *   If it's an implied action announcement, and the next one
             *   qualifies for group inclusion, build a group.  Note that
             *   because we only loop until we reach the second-to-last
             *   item, we know for sure there is indeed a next item to
             *   index here.  
             */
            if (cur.ofKind(ImplicitActionAnnouncement)
                && canGroupWith(cur, vec[i+1]))
            {
                local j;
                local groupVec;

                /* create a vector to hold the re-sorted group listing */
                groupVec = new Vector(16);

                /* 
                 *   Scan items for grouping.  This time, we want to scan
                 *   to the last (not second-to-last) item in the main
                 *   list, since we could conceivably group everything
                 *   remaining. 
                 */
                for (j = i ; j <= len ; )
                {
                    /* get this item */
                    cur = vec[j];
                    
                    /* unstack any recursive grouping */
                    j = unstackRecursiveGroup(groupVec, vec, j);

                    /* 
                     *   if we've used now everything in the list, or the
                     *   next item can't be grouped with the current item,
                     *   we're done 
                     */
                    if (j > len || !canGroupWith(cur, vec[j]))
                        break;
                }

                /* process default object announcements */
                processDefaultAnnouncements(groupVec);

                /* build the composite message for the entire group */
                vec[i].messageText_ = implicitAnnouncementGrouper
                    .compositeMessage(groupVec);

                /* 
                 *   Clear the messages in the second through last grouped
                 *   announcements.  Leave the report objects themselves
                 *   intact, so that our internal structural record of the
                 *   transcript remains as it was, but make them silent in
                 *   the displayed text, since these messages are now
                 *   subsumed into the combined first message.  
                 */
                for (++i ; i < j ; ++i)
                    vec[i].messageText_ = '';

                /* 
                 *   continue the main loop from the next element after the
                 *   last one we included in the group 
                 */
                i = j - 1;
            }

            /*
             *   If this is an implied action or default object
             *   announcement that interrupts a set of regular command
             *   reports, and it's for an action nested within the action
             *   generating the reports, add a paragraph spacer before the
             *   implicit announcement.  
             */
            if ((cur.ofKind(ImplicitActionAnnouncement)
                 || cur.ofKind(DefaultObjectAnnouncement))
                && cur.messageText_ != '')
            {
                local j;
                
                /* scan back for the nearest announcement with text */
                for (j = origI - 1 ; j >= 1 && vec[j].messageText_ == '' ;
                     --j) ;

                /* 
                 *   if it's a regular command report, and our implied or
                 *   default announcement is nested within it, add a
                 *   paragraph spacer 
                 */
                if (j >= 1
                    && vec[j].ofKind(FullCommandReport)
                    && cur.isActionNestedIn(vec[j]))
                {
                    /* 
                     *   insert a paragraph spacer before the announcement
                     *   - this will make the implied action and its
                     *   results stand out as separate actions, rather than
                     *   running everything together without spacing 
                     */
                    vec.insertAt(origI, new GroupSeparatorMessage(cur));

                    /* adjust our indices for the insertion */
                    ++i;
                    ++len;
                }
            }
        }
    }

    /*
     *   "Unstack" a recursive group of nested announcements.  Adds the
     *   recursive group to the output group vector in chronological order,
     *   and returns the index of the next item after the recursive group.
     *   
     *   A recursive group is a set of nested implicit commands, where one
     *   implicit command triggered another, which triggered another, and
     *   so on.  The innermost of the nested set is the one that's actually
     *   executed first chronologically, since an implied command must be
     *   carried out before its enclosing command can proceed.  For
     *   example:
     *   
     *.  >go south
     *.  (first opening the door)
     *.  (first unlocking the door)
     *.  (first taking the key out of the bag)
     *   
     *   Going south implies opening the door, but before we can open the
     *   door, we must unlock it, and before we can unlock it we must be
     *   holding the key.  In report order, the innermost command is listed
     *   last, since it's nested within the enclosing commands.
     *   Chronologically, though, the innermost command is actually
     *   executed first.  The purpose of this routine is to unstack these
     *   nested sets, rearranging them into chronological order.  
     */
    unstackRecursiveGroup(groupVec, vec, idx)
    {
        local cur;

        /* remember the item we're tasked to work on */
        cur = vec[idx];

        /* skip the current item */
        ++idx;
        
        /*
         *   Scan for items nested within vec[idx].  Process each child
         *   item first.  An item is nested within us if can be grouped
         *   with us, and its action is a child of our action.  
         */
        for (local len = vec.length() ; idx <= len ; )
        {
            /* if the next item is nested within 'cur', process it */
            if (canGroupWith(cur, vec[idx])
                && vec[idx].getAction().isNestedIn(cur.getAction()))
            {
                /* 
                 *   It's nested with us - process it recursively.  Since
                 *   our goal is to unstack these reports into
                 *   chronological order, we must process our children
                 *   first, so that they get added to the group vector
                 *   first, since children chronologically predede their
                 *   parents. 
                 */
                idx = unstackRecursiveGroup(groupVec, vec, idx);
            }
            else
            {
                /* it's not nested within us, so we're done */
                break;
            }
        }

        /* add our item to the result vector */
        groupVec.append(cur);

        /* 
         *   return the index of the next item; this is simply the current
         *   'idx' value, since we've advanced it past each item we've
         *   processed 
         */
        return idx;
    }

    /*
     *   Process default object announcements in a grouped message vector.
     *   
     *   Default object announcements come in two flavors: with and without
     *   message text.  Those without message text are present purely to
     *   retain a structural record of the default object in the internal
     *   transcript; we can simply remove these, since the actions that
     *   created them didn't even want default messages.  For those that do
     *   include message text, remove them as well, but also use their
     *   actions to replace the corresponding parent actions, so that the
     *   parent actions reflect what actually happened with the final
     *   defaulted objects.  
     */
    processDefaultAnnouncements(vec)
    {
        /* scan the vector for default announcements */
        for (local i = 1, local len = vec.length() ; i <= len ; ++i)
        {
            local cur = vec[i];
            
            /* if this is a default announcement, process it */
            if (cur.ofKind(DefaultObjectAnnouncement))
            {
                /* 
                 *   If it has a message, use its action to replace the
                 *   parent action.  The only way an implied command can
                 *   have a defaulted object is for the implied command to
                 *   have been stated with too few objects, so that an
                 *   askForIobj (for example) occurred.  In such cases, the
                 *   default announcement will be a child action of the
                 *   original underspecified action, so we can simply find
                 *   the original action and replace it with the defaulted
                 *   action.
                 */
                if (cur.messageText_ != '')
                {
                    /* 
                     *   Scan for the parent announcement.
                     *   
                     *   Note that the implicit announcement containing the
                     *   parent action will follow the default announcement
                     *   in the result list, since the default announcement
                     *   is a child of the parent.  
                     */
                    for (local j = i + 1 ; j <= len ; ++j)
                    {
                        /* if this is the parent action, replace it */
                        if (vec[j].getAction()
                            == cur.getAction().parentAction)
                        {
                            /* this is it - replace the action */
                            vec[j].setAction(cur.getAction());
                            
                            /* no need to look any further */
                            break;
                        }
                    }
                }

                /* remove the default announcement from the list */
                vec.removeElementAt(i);
                
                /* adjust our list index and length for the deletion */
                --i;
                --len;
            }
        }
    }

    /* 
     *   Can we group the second item with the first?  Returns true if the
     *   second item is also an implicit action announcement, or it's a
     *   default object announcement whose parent action is the first
     *   item's action. 
     */
    canGroupWith(a, b)
    {
        /* if 'b' is also an implicit announcement, we can include it */
        if (b.ofKind(ImplicitActionAnnouncement))
            return true;

        /* 
         *   if 'b' is a default object announcement, and has the same
         *   parent action as 'a', then we can group it; otherwise we can't
         */
        return (b.ofKind(DefaultObjectAnnouncement)
                && b.getAction().parentAction == a.getAction());
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Transcript Transform: Complex Multi-object Separation.  If we have an
 *   action that's being applied to one of a bunch of iterated objects, and
 *   the action has any implied command announcements associated with it,
 *   we'll set off the result for this command from its preceding and
 *   following commands by a paragraph separator.  
 */
complexMultiTransform: TranscriptTransform
    applyTransform(trans, vec)
    {
        /* scan the list for multi-object announcements */
        foreach (local cur in vec)
        {
            /* if it's a multi-object announcement, check it */
            if (cur.ofKind(MultiObjectAnnouncement))
            {
                local idx;
                local cnt;
                local sep;
                    
                /* 
                 *   We have a multi-object announcement.  If we find only
                 *   one other report within the group, and the report's
                 *   text is short, let this report run together with its
                 *   neighbors without any additional visual separation.
                 *   Otherwise, set this group apart from its neighbors by
                 *   adding a paragraph break before and after the group;
                 *   this will make the results easier to read by visually
                 *   separating each longish response as a separate
                 *   paragraph.
                 *   
                 *   First, find the current item's index.  
                 */
                idx = vec.indexWhich({x: x == cur});

                /* 
                 *   now scan subsequent items in the same command
                 *   iteration, and check to see if (1) we have more than
                 *   one item, or (2) the item has a longish message 
                 */
                for (cnt = 0, ++idx, sep = nil ;
                     idx <= vec.length() && cnt < 2 ; ++idx, ++cnt)
                {
                    local sub = vec[idx];
                    
                    /* if we've reached the end of the group, stop scanning */
                    if (sub.iter_ != cur.iter_)
                        break;
                    
                    /* 
                     *   If it has long text, add visual separation.  Note
                     *   that "long" is just a heuristic, because we can't
                     *   tell whether the text will wrap in any given
                     *   interpreter - that depends on the width of the
                     *   interpreter window and the font size, among other
                     *   things, and we have no way of knowing any of this
                     *   here.  
                     */
                    if (sub.ofKind(CommandReportMessage)
                        && sub.messageText_ != nil
                        && sub.messageText_.length() > 60)
                    {
                        /* it's long - add separation */
                        sep = true;
                        break;
                    }
                }

                /* if we need separation, add it now */
                if (sep || cnt > 1)   
                {
                    /* 
                     *   This is indeed a complex iterated item.  Set it
                     *   off by paragraph breaks before and after the
                     *   iteration.
                     *   
                     *   First, find the first item in this iteration.  If
                     *   it's not the first item in the whole transcript,
                     *   insert a separator before it.  
                     */
                    idx = vec.indexWhich({x: x.iter_ == cur.iter_});
                    if (idx != 1)
                        vec.insertAt(idx, new GroupSeparatorMessage(cur));

                    /* 
                     *   Next, find the last item in this iteration.  If
                     *   it's no the last item in the entire transcript,
                     *   add a separator after it. 
                     */
                    idx = vec.lastIndexWhich({x: x.iter_ == cur.iter_});
                    if (idx != vec.length())
                        vec.insertAt(idx + 1, new GroupSeparatorMessage(cur));
                }
            }

            /* 
             *   if it's a command result from an implied command, and we
             *   have another command result following from the enclosing
             *   command, add a separator between this result and the next
             *   result 
             */
            if (cur.ofKind(CommandReportMessage) && cur.isActionImplicit)
            {
                local idx;

                /* get the index of this element */
                idx = vec.indexOf(cur);

                /* 
                 *   if there's another element following, check to see if
                 *   it's a command report for an enclosing action (i.e.,
                 *   an action that initiated this implied action) 
                 */
                if (idx < vec.length())
                {
                    local nxt;

                    /* get the next element */
                    nxt = vec[idx + 1];

                    /* 
                     *   if it's a command report for an action that
                     *   encloses this action, or it's another implicit
                     *   announcement, then put a separator before it 
                     */
                    if ((nxt.ofKind(CommandReportMessage)
                         && nxt.getAction() != cur.getAction()
                         && cur.isActionNestedIn(nxt))
                        || nxt.ofKind(ImplicitActionAnnouncement))
                         
                    {
                        /* add a separator */
                        vec.insertAt(idx + 1,
                                     new InternalSeparatorMessage(cur));
                    }
                }
            }
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Invoke a callback function using a transcript of the given class.
 *   Returns the return value of the callback function.  
 */
withCommandTranscript(transcriptClass, func)
{
    local transcript;
    local oldTranscript;

    /* 
     *   if we already have an active transcript, just invoke the
     *   function, running everything through the existing active
     *   transcript 
     */
    if (gTranscript != nil && gTranscript.isActive)
    {
        /* invoke the callback and return the result */
        return (func)();
    }

    /* 
     *   Create a transcript of the given class.  Make the transcript
     *   transient, since it's effectively part of the output stream state
     *   and thus shouldn't be saved or undone. 
     */
    transcript = transcriptClass.createTransientInstance();

    /* make this transcript the current global transcript */
    oldTranscript = gTranscript;
    gTranscript = transcript;

    /* install the transcript as a filter on the main output stream */
    mainOutputStream.addOutputFilter(transcript);

    /* make sure we undo our global changes before we leave */
    try
    {
        /* invoke the callback and return the result */
        return (func)();
    }
    finally
    {
        /* uninstall the transcript output filter */
        mainOutputStream.removeOutputFilter(transcript);

        /* restore the previous global transcript */
        gTranscript = oldTranscript;

        /* show the transcript results */
        transcript.showReports(true);
    }
}

