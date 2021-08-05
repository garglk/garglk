#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: command execution
 *   
 *   This module defines functions that perform command execution.  
 */

#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Execute a command line, as issued by the given actor and as given as a
 *   list of tokens.
 *   
 *   If 'firstInSentence' is true, we're at the start of a "sentence."  The
 *   meaning and effect of this may vary by language.  In English, a
 *   sentence ends with certain punctuation marks (a period, semicolon,
 *   exclamation mark, or question mark), so anything after one of these
 *   punctuation marks is the start of a new sentence.  Also in English, we
 *   can address a command to an explicit target actor using the "actor,"
 *   prefix syntax, which we can't use except at the start of a sentence.
 *   
 *   If the command line consists of multiple commands, we will only
 *   actually execute the first command before returning.  We'll schedule
 *   any additional commands for later execution by putting them into the
 *   target actor's pending command queue before we return, but we won't
 *   actually execute them.  
 */
executeCommand(targetActor, issuingActor, toks, firstInSentence)
{
    local actorPhrase;
    local actorSpecified;

    /*
     *   Turn on sense caching while we're working, until execution
     *   begins.  The parsing and resolution phases of command processing
     *   don't involve any changes to game state, so we can safely cache
     *   sense information; caching sense information during these phases
     *   is desirable because these steps (noun resolution in particular)
     *   involve repeated inspection of the current sensory environment,
     *   which can require expensive calculations.  
     */
    libGlobal.enableSenseCache();

    /* we don't have an explicit actor phrase yet */
    actorPhrase = nil;
    
    /* presume an actor will not be specified */
    actorSpecified = nil;
    
    /*
     *   If this is the start of a new sentence, and the issuing actor
     *   wants to cancel any target actor designation at the end of each
     *   sentence, change the target actor back to the issuing actor.  
     */
    if (firstInSentence
        && issuingActor != targetActor
        && issuingActor.revertTargetActorAtEndOfSentence)
    {
        /* switch to the target actor */
        targetActor = issuingActor;

        /* switch to the issuer's sense context */
        senseContext.setSenseContext(issuingActor, sight);
    }

    /*
     *   Keep going until we've processed the command.  This might take
     *   several iterations, because we might have replacement commands to
     *   execute.  
     */
parseTokenLoop:
    for (;;)
    {
        local lst;
        local action;
        local match;
        local nextIdx;
        local nextCommandTokens;
        local extraIdx;
        local extraTokens;
        
        /*
         *   Catch any errors that occur while executing the command,
         *   since some of them are signals to us that we should reparse
         *   some new text read or generated deep down inside the command
         *   processing.  
         */
        try
        {
            local rankings;

            /* we have no extra tokens yet */
            extraTokens = [];

            /* 
             *   Parse the token list.  If this is the first command on
             *   the command line, allow an actor prefix.  Otherwise, just
             *   look for a command.  
             */
            lst = (firstInSentence ? firstCommandPhrase : commandPhrase)
                  .parseTokens(toks, cmdDict);

            /*
             *   As a first cut at reducing the list of possible matches
             *   to those that make sense, eliminate from this list any
             *   matches which do not have valid actions.  In grammars for
             *   "scrambling" languages (i.e., languages with flexible
             *   word ordering), it is possible to construct commands that
             *   fit the grammatical rules of sentence construction but
             *   which make no sense because of the specific constraints
             *   of the verbs involved; we can filter out such nonsense
             *   interpretations immediately by keeping only those
             *   structural interpretations that can resolve to valid
             *   actions.  
             */
            lst = lst.subset(
                {x: x.resolveFirstAction(issuingActor, targetActor) != nil});

            /* if we have no matches, the command isn't understood */
            if (lst.length() == 0)
            {
                /* 
                 *   If this is a first-in-sentence phrase, try looking for
                 *   a target actor phrase.  If we can find one, we can
                 *   direct the 'oops' to that actor, to allow the game to
                 *   customize messages more specifically.  
                 */
                if (firstInSentence)
                {
                    local i;
                    
                    /* try parsing an "actor, <unknown>" phrase */
                    lst = actorBadCommandPhrase.parseTokens(toks, cmdDict);

                    /* if we got any matches, try to resolve actors */
                    lst = lst.mapAll({x: x.resolveNouns(
                        issuingActor, issuingActor,
                        new TryAsActorResolveResults())});

                    /* drop any that didn't yield any results */
                    lst = lst.subset({x: x != nil && x.length() != 0});

                    /*
                     *
                     *   if anything's left, and one of the entries 
                     *   resolves to an actor, arbitrarily pick the 
                     *   first such entry use the resolved actor as the 
                     *   new target actor 
                     */
                    if (lst.length() != 0
                        && (i = lst.indexWhich(
                            {x: x[1].obj_.ofKind(Actor)})) != nil)
                        targetActor = lst[i][1].obj_;
                }

                /* 
                 *   We don't understand the command.  Check for unknown
                 *   words - if we have any, give them a chance to use
                 *   OOPS to correct a typo.  
                 */
                tryOops(toks, issuingActor, targetActor,
                        1, toks, rmcCommand);

                /* 
                 *   try running it by the SpecialTopic history to see if
                 *   they're trying to use a special topic in the wrong
                 *   context - if so, explain that they can't use the
                 *   command right now, rather than claiming that the
                 *   command is completely invalid 
                 */
                if (specialTopicHistory.checkHistory(toks))
                {
                    /* the special command is not currently available */
                    targetActor.notifyParseFailure(
                        issuingActor, &specialTopicInactive, []);
                }
                else
                {
                    /* tell the issuer we didn't understand the command */
                    targetActor.notifyParseFailure(
                        issuingActor, &commandNotUnderstood, []);
                }
                
                /* 
                 *   we're done with this command, and we want to abort
                 *   any subsequent commands on the command line 
                 */
                return;
            }

            /* show the matches if we're in debug mode */
            dbgShowGrammarList(lst);
        
            /* 
             *   Perform a tentative resolution on each alternative
             *   structural interpretation of the command, and rank the
             *   interpretations in order of "goodness" as determined by
             *   our ranking criteria encoded in CommandRanking.
             *   
             *   Note that we perform the tentative resolution and ranking
             *   even if we have only one interpretation, because the
             *   tentative resolution is often useful for the final
             *   resolution pass.  
             */
            rankings = CommandRanking
                       .sortByRanking(lst, issuingActor, targetActor);
                
            /* 
             *   Take the interpretation that came up best in the rankings
             *   (or, if we have multiple at the same ranking, arbitrarily
             *   pick the one that happened to come up first in the list)
             *   - they're ranked in descending order of goodness, so take
             *   the first one.
             */
            match = rankings[1].match;

            /* if we're in debug mode, show the winner */
            dbgShowGrammarWithCaption('Winner', match);
            
            /*
             *   Get the token list for the rest of the command after what
             *   we've parsed so far.
             *   
             *   Note that we'll start over and parse these tokens anew,
             *   even though we might have parsed them already into a
             *   subcommand on the previous iteration.  Even if we already
             *   parsed these tokens, we want to parse them again, because
             *   we did not have a suitable context for evaluating the
             *   semantic strengths of the possible structural
             *   interpretations this command on the previous iteration;
             *   for example, it was not possible to resolve noun phrases
             *   in this command because we could not guess what the scope
             *   would be when we got to this point.  So, we'll simply
             *   discard the previous match tree, and start over, treating
             *   the new tokens as a brand new command.  
             */
            nextIdx = match.getNextCommandIndex();
            nextCommandTokens = toks.sublist(nextIdx);

            /* if the pending command list is empty, make it nil */
            if (nextCommandTokens.length() == 0)
                nextCommandTokens = nil;

            /*
             *   Get the part of the token list that the match doesn't use
             *   at all.  These are the tokens following the tokens we
             *   matched. 
             */
            extraIdx = match.tokenList.length() + 1;
            extraTokens = toks.sublist(extraIdx);

            /*
             *   We now have the best match for the command tree.
             *   
             *   If the command has an actor clause, resolve the actor, so
             *   that we can direct the command to that actor.  If the
             *   command has no actor clause, the command is to the actor
             *   who issued the command.
             *   
             *   Do NOT process the actor clause if we've already done so.
             *   If we edit the token list and retry the command after
             *   this point, there is no need to resolve the actor part
             *   again.  Doing so could be bad - if resolving the actor
             *   required player interaction, such as asking for help with
             *   an ambiguous noun phrase, we do not want to go through
             *   the same interaction again just because we have to edit
             *   and retry a later part of the command.  
             */
            if (match.hasTargetActor())
            {
                local actorResults;

                /*
                 *   If we haven't yet explicitly specified a target
                 *   actor, and the default target actor is different from
                 *   the issuing actor, then this is really a new command
                 *   from the issuing actor.
                 *   
                 *   First, an actor change mid-command is allowed only if
                 *   the issuing actor waits for NPC commands to be
                 *   carried out; if not, then we can't change the actor
                 *   mid-command.
                 *   
                 *   Second, since the command comes from the issuing
                 *   actor, not from the default target actor, we want to
                 *   issue the orders on the issuing actor's turn, so put
                 *   the command into the queue for the issuer, and let
                 *   the issuer re-parse the command on the issuer's next
                 *   turn.  
                 */
                if (!actorSpecified && issuingActor != targetActor)
                {
                    /* 
                     *   don't allow the command if the issuing actor
                     *   doesn't wait for orders to be completed 
                     */
                    if (!issuingActor.issueCommandsSynchronously)
                    {
                        /* turn off any sense capturing */
                        senseContext.setSenseContext(nil, sight);

                        /* show the error */
                        issuingActor.getParserMessageObj()
                            .cannotChangeActor();

                        /* done */
                        return;
                    }

                    /* put the command into the issuer's queue */
                    issuingActor.addFirstPendingCommand(
                        firstInSentence, issuingActor, toks);

                    /* done */
                    return;
                }

                /* create an actor-specialized results object */
                actorResults = new ActorResolveResults();
                
                /* set up the actors in the results object */
                actorResults.setActors(targetActor, issuingActor);
                
                /* resolve the actor object */
                match.resolveNouns(issuingActor, targetActor, actorResults);

                /* get the target actor from the command */
                targetActor = match.getTargetActor();

                /* pull out the phrase specifying the actor */
                actorPhrase = match.getActorPhrase();

                /*
                 *   Copy antecedents from the issuing actor to the target
                 *   actor.  Since the issuer is giving us a new command
                 *   here, pronouns will be given from the issuer's
                 *   perspective.  
                 */
                targetActor.copyPronounAntecedentsFrom(issuingActor);

                /* let the actor phrase know we're actually using it */
                match.execActorPhrase(issuingActor);
                
                /* 
                 *   Ask the target actor if it's interested in the
                 *   command at all.  This only applies when the actor was
                 *   actually specified - if an actor wasn't specified,
                 *   the command is either directed to the issuer itself,
                 *   in which case the command will always be accepted; or
                 *   the command is a continuation of a command line
                 *   previously accepted.  
                 */
                if (!targetActor.acceptCommand(issuingActor))
                {
                    /* 
                     *   the command was immediately rejected - abandon
                     *   the command and any subsequent commands on the
                     *   same line 
                     */
                    return;
                }

                /* note that an actor was specified */
                actorSpecified = true;

                /*
                 *   Pull out the rest of the command (other than the
                 *   target actor specification) and start over with a
                 *   fresh parse of the whole thing.  We must do this
                 *   because our tentative resolution pass that we used to
                 *   pick the best structural interpretation of the
                 *   command couldn't go far enough - since we didn't know
                 *   the actor involved, we weren't able to resolve nouns
                 *   in the rest of the command.  Now that we know the
                 *   actor, we can start over and resolve everything in
                 *   the rest of the command, and thus choose the right
                 *   structural match for the command.  
                 */
                toks = match.getCommandTokens();

                /* what follows obviously isn't first in the sentence */
                firstInSentence = nil;

                /* go back to parse the rest */
                continue parseTokenLoop;
            }

            /* pull out the first action from the command */        
            action = match.resolveFirstAction(issuingActor, targetActor);

            /*
             *   If the winning interpretation had any unknown words, run
             *   a resolution pass to resolve those interactively, if
             *   possible.  We want to do this before doing any other
             *   interactive resolution because OOPS has the unique
             *   property of forcing us to reparse the command; if we
             *   allowed any other interactive resolution to happen before
             *   processing an OOPS, we'd likely have to repeat the other
             *   resolution on the reparse, which would confuse and
             *   irritate users by asking the same question more than once
             *   for what is apparently the same command.  
             */
            if (rankings[1].unknownWordCount != 0)
            {
                /* 
                 *   resolve using the OOPS results gatherer - this runs
                 *   essentially the same preliminary resolution process
                 *   as the ranking results gatherer, but does perform
                 *   interactive resolution of unknown words via OOPS 
                 */
                match.resolveNouns(
                    issuingActor, targetActor,
                    new OopsResults(issuingActor, targetActor));
            }

            /* 
             *   If the command is directed to a different actor than the
             *   issuer, change to the target actor's sense context.  
             *   
             *   On the other hand, if this is a conversational command
             *   (e.g., BOB, YES or BOB, GOODBYE), execute it within the
             *   sense context of the issuer, even when a target is
             *   specified.  Target actors in conversational commands
             *   designate the interlocutor rather than the performing
             *   actor: BOB, YES doesn't ask Bob to say "yes", but rather
             *   means that the player character (the issuer) is saying
             *   "yes" to Bob.
             */
            if (action != nil && action.isConversational(issuingActor))
                senseContext.setSenseContext(issuingActor, sight);
            else if (actorSpecified && targetActor != issuingActor)
                senseContext.setSenseContext(targetActor, sight);

            /* set up a transcript to receive the command results */
            withCommandTranscript(CommandTranscript, function()
            {
                /* 
                 *   Execute the action.
                 *   
                 *   If a target actor was specified, and it's not the same
                 *   as the issuing actor, this counts as a turn for the
                 *   issuing actor.  
                 */
                executeAction(targetActor, actorPhrase, issuingActor,
                              actorSpecified && issuingActor != targetActor,
                              action);
            });

            /*
             *   If we have anything remaining on the command line, insert
             *   the remaining commands at the start of the target actor's
             *   command queue, since we want the target actor to continue
             *   with the rest of this command line as its next operation.
             *   
             *   Since the remainder of the line represents part of the
             *   work the actor pulled out of the queue in order to call
             *   us, put the remainder back in at the START of the actor's
             *   queue - it was before anything else that might be in the
             *   actor's queue before, so it should stay ahead of anything
             *   else now.  
             */
            if (nextCommandTokens != nil)
            {
                /* 
                 *   Prepend the remaining commands to the actor's queue.
                 *   The next command is the start of a new sentence if
                 *   the command we just executed ends the sentence. 
                 */
                targetActor.addFirstPendingCommand(
                    match.isEndOfSentence(), issuingActor, nextCommandTokens);
            }

            /*
             *   If the command was directed from the issuer to a
             *   different target actor, and the issuer wants to wait for
             *   the full set of issued commands to complete before
             *   getting another turn, tell the issuer to begin waiting.  
             */
            if (actorSpecified && issuingActor != targetActor)
                issuingActor.waitForIssuedCommand(targetActor);

            /* we're done */
            return;
        }
        catch (ParseFailureException rfExc)
        {
            /*
             *   Parsing failed in such a way that we cannot proceed.
             *   Tell the target actor to notify the issuing actor.  
             */
            rfExc.notifyActor(targetActor, issuingActor);

            /* 
             *   the command cannot proceed, so abandon any remaining
             *   tokens on the command line 
             */
            return;
        }
        catch (CancelCommandLineException cclExc)
        {
            /* 
             *   if there are any tokens remaining, the game might want to
             *   show an explanation 
             */
            if (nextCommandTokens != nil)
                targetActor.getParserMessageObj().explainCancelCommandLine();

            /* stop now, abandoning the rest of the command line */	
            return;
        }
        catch (TerminateCommandException tcExc)
        {
            /* 
             *   the command cannot proceed - we can't do any more
             *   processing of this command, so simply return, abandoning
             *   any additional tokens we have 
             */
            return;
        }
        catch (RetryCommandTokensException rctExc)
        {
            /* 
             *   We want to replace the current command's token list with
             *   the new token list - get the new list, and then go back
             *   and start over processing it.
             *   
             *   Note that we must retain any tokens beyond those that the
             *   match tree used.  The exception only edits the current
             *   match tree's matched tokens, since it doesn't have access
             *   to any of the original tokens beyond those, so we must
             *   now add back in any tokens beyond the originals.  
             */
            toks = rctExc.newTokens_ + extraTokens;

            /* go back and process the command again with the new tokens */
            continue parseTokenLoop;
        }
        catch (ReplacementCommandStringException rcsExc)
        {
            local str;
            
            /* retrieve the new command string from the exception */
            str = rcsExc.newCommand_;

            /* 
             *   if the command string is nil, it means that the command
             *   has been fully handled already, so we simply return
             *   without any further work 
             */
            if (str == nil)
                return;

            /* 
             *   Replace the entire command string with the one from the
             *   exception - this cancels any previous command that we
             *   had.
             */
            toks = cmdTokenizer.tokenize(str);

            /* 
             *   we have a brand new command line, so we're starting a
             *   brand new sentence 
             */
            firstInSentence = true;

            /* set the issuing and target actor according to the exception */
            issuingActor = rcsExc.issuingActor_;
            targetActor = rcsExc.targetActor_;

            /* 
             *   Put this work into the target actor's work queue, so that
             *   the issuer will carry out the command at the next
             *   opportunity.  This is a brand new command line, so it
             *   starts a new sentence.  
             */
            targetActor.addPendingCommand(true, issuingActor, toks);

            /* we're done processing this command */
            return;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   GlobalRemapping makes it possible to transform one action into another
 *   globally - as opposed to the remapTo mechanism, which lets an object
 *   involved in the command perform a remapping.  The key difference
 *   between global remappings and remapTo is that the latter can't happen
 *   until after the objects are resolved (for fairly obvious reasons: each
 *   remapTo mapping is associated with an object, so you can't know which
 *   mapping to apply until you know which object is involved).  In
 *   contrast, global remappings are performed *before* object resolution -
 *   this is possible because the mappings don't depend on the objects
 *   involved in the action.
 *   
 *   Whenever an action is about to be executed, the parser runs through
 *   all of the defined global remappings, and gives each one a chance to
 *   remap the command.  If any remapping succeeds, we replace the original
 *   command with the remapped version, then repeat the scan of the global
 *   remapping list from the start - we do another complete scan of the
 *   list in case there's another global mapping that applies to the
 *   remapped version of the command.  We repeat this process until we make
 *   it through the whole list without finding a remapping.
 *   
 *   GlobalRemapping instances are added to the master list of mappings
 *   automatically at pre-init time, and any time you construct one
 *   dynamically with 'new'.
 */
class GlobalRemapping: PreinitObject
    /*
     *   Check for and apply a remapping.  This method must be implemented
     *   in each GlobalRemapping instance to perform the actual remapping
     *   work.
     *   
     *   This routine should first check to see if the command is relevant
     *   to this remapping.  In most cases, this means checking that the
     *   command matches some template, such as having a particular action
     *   (verb) and combination of potential objects.  Note that the
     *   objects aren't fully resolved during global remapping - the whole
     *   point of global remapping is to catch certain phrasings before we
     *   get to the noun resolution phase - but the *phrases* involved will
     *   be known, so the range of potential matches is knowable.
     *   
     *   If the routine decides that the action isn't relevant to this
     *   remapping, it should simply return nil.
     *   
     *   If the action decides to remap the action, it must create a new
     *   Action object representing the replacement version of the command.
     *   Then, return a list, [targetActor, action], giving the new target
     *   actor and the new action.  You don't have to change the target
     *   actor, of course, but it's included in the result so that you can
     *   change it if you want to.  For example, you could use this to
     *   remap a command of the form "X, GIVE ME Y" to "ME, ASK X FOR Y" -
     *   note that the target actor changes from X to ME.  
     */
    getRemapping(issuingActor, targetActor, action)
    {
        /* 
         *   this must be overridden to perform the actual remapping; by
         *   default, simply return nil to indicate that we don't want to
         *   remap this action 
         */
        return nil;
    }

    /*
     *   Remapping order - the parser applies global remappings in
     *   ascending order of this value.  In most cases, the order shouldn't
     *   matter, since most remappings should be narrow enough that a given
     *   command will only be subject to one remapping rule.  However, in
     *   some cases you might need to define rules that overlap, so the
     *   ordering lets you specify which one goes first.  In most cases
     *   you'll want to apply the more specific rule first. 
     */
    remappingOrder = 100

    /* 
     *   Static class method: look for a remapping.  This runs through the
     *   master list of mappings, looking for a mapping that applies to the
     *   given command.  If we find one, we'll replace the command with the
     *   remapped version, then start over with a fresh scan of the entire
     *   list to see if there's a remapping for the *new* version of the
     *   command.  We repeat this until we get through the whole list
     *   without finding a remapping.
     *   
     *   The return value is a list, [targetActor, action], giving the
     *   resulting target actor and new action object.  If we don't find
     *   any remapping, this will simply be the original values passed in
     *   as our arguments; if we do find a remapping, this will be the new
     *   version of the command.  
     */
    findGlobalRemapping(issuingActor, targetActor, action)
    {
        /* get the global remapping list */
        local v = GlobalRemapping.allGlobalRemappings;
        local cnt = v.length();

        /* if necessary, sort the list */
        if (GlobalRemapping.listNeedsSorting)
        {
            /* sort it by ascending remappingOrder value */
            v.sort(SortAsc, {a, b: a.remappingOrder - b.remappingOrder});

            /* note that it's now sorted */
            GlobalRemapping.listNeedsSorting = nil;
        }
        
        /* 
         *   iterate through the list repeatedly, until we make it all the
         *   way through without finding a mapping 
         */
        for (local done = nil ; !done ; )
        {
            /* presume we won't find a remapping on this iteration */
            done = true;

            /* run through the list, looking for a remapping */
            for (local i = 1 ; i <= cnt ; ++i)
            {
                local rm;
                
                /* check for a remapping */
                rm = v[i].getRemapping(issuingActor, targetActor, action);
                if (rm != nil)
                {
                    /* found a remapping - apply it */
                    targetActor = rm[1];
                    action = rm[2];

                    /* 
                     *   we found a remapping, so we have to repeat the
                     *   scan of the whole list - note that we're not done,
                     *   and break out of the current scan so that we start
                     *   over with a fresh scan 
                     */
                    done = nil;
                    break;
                }
            }
        }

        /* return the final version of the command */
        return [targetActor, action];
    }

    /* pre-initialization: add each instance to the master list */
    execute()
    {
        /* add me to the master list */
        registerGlobalRemapping();
    }

    /* construction: add myself to the master list */
    construct()
    {
        /* add me to the master list */
        registerGlobalRemapping();
    }        

    /* register myself with the global list, making this an active mapping */
    registerGlobalRemapping()
    {
        /* add myself to the global list */
        GlobalRemapping.allGlobalRemappings.append(self);

        /* note that a sort is required the next time we run */
        GlobalRemapping.listNeedsSorting = true;
    }

    /* 
     *   unregister - this removes me from the global list, making this
     *   mapping inactive: after being unregistered, the parser won't apply
     *   this mapping to new commands 
     */
    unregisterGlobalRemapping()
    {
        GlobalRemapping.allGlobalRemappings.removeElement(self);
    }

    /* 
     *   Static class property: the master list of remappings.  We build
     *   this automatically at preinit time, and manipulate it via our
     *   constructor.
     */
    allGlobalRemappings = static new Vector(10)

    /* 
     *   static class property: the master list needs to be sorted; this is
     *   set to true each time we update the list, so that the list scanner
     *   knows to sort it before doing its scan 
     */
    listNeedsSorting = nil
;



/* ------------------------------------------------------------------------ */
/*
 *   Execute an action, as specified by an Action object.  We'll resolve
 *   the nouns in the action, then perform the action.
 */
executeAction(targetActor, targetActorPhrase,
              issuingActor, countsAsIssuerTurn, action)
{
    local rm, results;

startOver:
    /* check for a global remapping */
    rm = GlobalRemapping.findGlobalRemapping(
        issuingActor, targetActor, action);
    targetActor = rm[1];
    action = rm[2];

    /* create a basic results object to handle the resolution */
    results = new BasicResolveResults();
        
    /* set up the actors in the results object */
    results.setActors(targetActor, issuingActor);

    /* catch any "remap" signals while resolving noun phrases */
    try
    {
        /* resolve noun phrases */
        action.resolveNouns(issuingActor, targetActor, results);
    }
    catch (RemapActionSignal sig)
    {
        /* mark the new action as remapped */
        sig.action_.setRemapped(action);
        
        /* get the new action from the signal */
        action = sig.action_;

        /* start over with the new action */
        goto startOver;
    }

    /*
     *   Check to see if we should create an undo savepoint for the
     *   command.  If the action is not marked for inclusion in the undo
     *   log, there is no need to log a savepoint for it.
     *   
     *   Don't save undo for nested commands.  A nested command is part of
     *   a main command, and we only want to save undo for the main
     *   command, not for its individual sub-commands.  
     */
    if (action.includeInUndo
        && action.parentAction == nil
        && (targetActor.isPlayerChar()
            || (issuingActor.isPlayerChar() && countsAsIssuerTurn)))
    {
        /* 
         *   Remember the command we're about to perform, so that if we
         *   undo to here we'll be able to report what we undid.  Note that
         *   we do this *before* setting the savepoint, because we want
         *   after the undo to know the command we were about to issue at
         *   the savepoint.  
         */
        libGlobal.lastCommandForUndo = action.getOrigText();
        libGlobal.lastActorForUndo =
            (targetActorPhrase == nil
             ? nil
             : targetActorPhrase.getOrigText());
            
        /* 
         *   set a savepoint here, so that we on 'undo' we'll restore
         *   conditions to what they were just before we executed this
         *   command 
         */
        savepoint();
    }
    
    /* 
     *   If this counts as a turn for the issuer, adjust the issuer's busy
     *   time.
     *   
     *   However, this doesn't apply if the command is conversational (that
     *   is, it's something like "BOB, HELLO").  A conversational command
     *   is conceptually carried out by the issuer, not the target actor,
     *   since the action consists of the issuer actually saying something
     *   to the target actor.  The normal turn accounting in Action will
     *   count a conversational command this way, so we don't have to do
     *   the extra bookkeeping for such a command here.
     */
    if (countsAsIssuerTurn && !action.isConversational(issuingActor))
    {
        /* 
         *   note in the issuer that the target is the most recent
         *   conversational partner 
         */
        issuingActor.lastInterlocutor = targetActor;
        
        /* make the issuer busy for the order-giving interval */
        issuingActor.addBusyTime(nil,
                                 issuingActor.orderingTime(targetActor));
        
        /* notify the target that this will be a non-idle turn */
        targetActor.nonIdleTurn();
    }

    /*
     *   If the issuer is directing the command to a different actor, and
     *   it's not a conversational command, check with the target actor to
     *   see if it wants to accept the command.  Don't check
     *   conversational commands, since these aren't of the nature of
     *   orders to be obeyed.  
     */
    if (issuingActor != targetActor
        && !action.isConversational(issuingActor)
        && !targetActor.obeyCommand(issuingActor, action))
    {
        /* 
         *   if the issuing actor's "ordering time" is zero, make this take
         *   up a turn anyway, just for the refusal 
         */
        if (issuingActor.orderingTime(targetActor) == 0)
            issuingActor.addBusyTime(nil, 1);

        /*
         *   Since we're aborting the command, we won't get into the normal
         *   execution for it.  However, we might still want to save it for
         *   an attempted re-issue with AGAIN, so do so explicitly now. 
         */
        action.saveActionForAgain(issuingActor, countsAsIssuerTurn,
                                  targetActor, targetActorPhrase);

        /* 
         *   This command was rejected, so don't process it any further,
         *   and give up on processing any remaining commands on the same
         *   command line. 
         */
        throw new TerminateCommandException();
    }
    
    /* execute the action */
    action.doAction(issuingActor, targetActor, targetActorPhrase,
                    countsAsIssuerTurn);
}

/* ------------------------------------------------------------------------ */
/*
 *   Try an implicit action.
 *   
 *   Returns true if the action was attempted, whether or not it
 *   succeeded, nil if the command was not even attempted.  We will not
 *   attempt an implied command that verifies as "dangerous," since this
 *   means that it should be obvious to the player character that such a
 *   command should not be performed lightly.  
 */
_tryImplicitAction(issuingActor, targetActor, msgProp, actionClass, [objs])
{
    local action;
    
    /* create an instance of the desired action class */
    action = actionClass.createActionInstance();

    /* mark the action as implicit */
    action.setImplicit(msgProp);

    /* install the resolved objects in the action */
    action.setResolvedObjects(objs...);

    /* 
     *   For an implicit action, we must check the objects involved to make
     *   sure they're in scope.  If any of the objects aren't in scope,
     *   there is no way the actor would know to perform the command, so
     *   the command would not be implied in the first place.  Simply fail
     *   without trying the command.  
     */
    if (!action.resolvedObjectsInScope())
        return nil;

    /* 
     *   catch the abort-implicit signal, so we can turn it into a result
     *   code for our caller instead of an exception 
     */
    try
    {
        /* in NPC mode, add a command separator before each implied action */
        if (targetActor.impliedCommandMode() == ModeNPC)
            gTranscript.addCommandSep();

        /* execute the action */
        action.doAction(issuingActor, targetActor, nil, nil);
        
        /* 
         *   if the actor is in "NPC" mode for implied commands, do some
         *   extra work 
         */
        if (targetActor.impliedCommandMode() == ModeNPC)
        {
            /*   
             *   we're in NPC mode, so if the implied action failed, then
             *   act as though the command had never been attempted 
             */
            if (gTranscript.actionFailed(action))
            {
                /* the implied command failed - act like we didn't even try */
                return nil;
            }

            /* 
             *   In "NPC" mode, we display the results from implied
             *   commands as though they had been explicitly entered as
             *   separate actions.  So, add visual separation after the
             *   results from the implied command. 
             */
            gTranscript.addCommandSep();
        }

        /* tell the caller we at least tried to execute the command */
        return true;
    }
    catch (AbortImplicitSignal sig)
    {
        /* tell the caller we didn't execute the command at all */
        return nil;
    }
    catch (ParseFailureException exc)
    {
        /*
         *   Parse failure.  If the actor is in NPC mode, we can't have
         *   asked for a new command, so this must be some failure in
         *   processing the implied command itself; most likely, we tried
         *   to resolve a missing object or the like and found that we
         *   couldn't perform interactive resolution (because of the NPC
         *   mode).  In this case, simply treat this as a failure of the
         *   implied command itself, and act as though we didn't even try
         *   the implied command.
         *   
         *   If the actor is in player mode, then we *can* perform
         *   interactive resolution, so we won't have thrown a parser
         *   failure before trying to solve the problem interactively.  The
         *   failure must therefore be in an interactive response.  In this
         *   case, simply re-throw the failure so that it reaches the main
         *   parser. 
         */
        if (targetActor.impliedCommandMode() == ModeNPC)
        {
            /* NPC mode - the implied command itself failed */
            return nil;
        }
        else
        {
            /* player mode - interactive resolution failed */
            throw exc;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Run a replacement action. 
 */
_replaceAction(actor, actionClass, [objs])
{
    /* run the replacement action as a nested action */
    _nestedAction(true, actor, actionClass, objs...);

    /* the invoking command is done */
    exit;
}

/* ------------------------------------------------------------------------ */
/*
 *   Resolve and execute a replacement action.  This differs from the
 *   normal replacement action execution in that the action we execute
 *   requires resolution before execution.  
 */
resolveAndReplaceAction(newAction)
{
    /* prepare the replacement action */
    prepareNestedAction(true, nil, newAction);
    
    /* 
     *   resolve and execute the new action, using the same target and
     *   issuing actors as the original action 
     */
    executeAction(gActor, nil, gIssuingActor, nil, newAction);
    
    /* the invoking command has been replaced, so it's done */
    exit;
}

/* ------------------------------------------------------------------------ */
/*
 *   Run an action as a new turn.  Returns the CommandTranscript describing
 *   the action's results.
 */
_newAction(transcriptClass, issuingActor, targetActor, actionClass, [objs])
{
    local action;

    /* create an instance of the desired action class */
    action = actionClass.createActionInstance();

    /* execute the command with the action instance */
    return newActionObj(transcriptClass, issuingActor, targetActor,
                        action, objs...);
}

/*
 *   Run an action as a new turn.  This is almost the same as _newAction,
 *   but should be used when the caller has already explicitly created an
 *   instance of the Action to be performed.
 *   
 *   If issuingActor is nil, we'll use the current global issuing actor; if
 *   that's also nil, we'll use the target actor.
 *   
 *   Returns a CommandTranscript object describing the result of the
 *   action.  
 */
newActionObj(transcriptClass, issuingActor, targetActor, actionObj, [objs])
{
    /* create the results object and install it as the global transcript */
    return withCommandTranscript(transcriptClass, function()
    {
        /* install the resolved objects in the action */
        actionObj.setResolvedObjects(objs...);

        /* 
         *   if the issuing actor isn't specified, use the current global
         *   issuing actor; if that's also not set, use the target actor 
         */
        if (issuingActor == nil)
            issuingActor = gIssuingActor;
        if (issuingActor == nil)
            issuingActor = targetActor;
        
        /* 
         *   Execute the given action.  Because this is a new action,
         *   execute the action in a new sense context for the given actor.
         */
        callWithSenseContext(targetActor.isPlayerChar()
                             ? nil : targetActor, sight,
            {: actionObj.doAction(issuingActor, targetActor, nil, nil)});

        /* return the current global transcript object */
        return gTranscript;
    });
}

/* ------------------------------------------------------------------------ */
/*
 *   Run a nested action.  'isReplacement' has the same meaning as in
 *   execNestedAction(). 
 */
_nestedAction(isReplacement, actor, actionClass, [objs])
{
    local action;

    /* create an instance of the desired action class */
    action = actionClass.createActionInstance();

    /* install the resolved objects in the action */
    action.setResolvedObjects(objs...);

    /* execute the new action */
    execNestedAction(isReplacement, nil, actor, action);
}

/*
 *   Execute a fully-constructed nested action.
 *   
 *   'isReplacement' indicates whether the action is a full replacement or
 *   an ordinary nested action.  If it's a replacement, then we use the
 *   game time taken by the replacement, and set the enclosing action
 *   (i.e., the current gAction) to take zero time.  If it's an ordinary
 *   nested action, then we consider the nested action to take zero time,
 *   using the current action's time as the overall command time.  
 *   
 *   'isRemapping' indicates whether or not this is a remapped action.  If
 *   we're remapping from one action to another, this will be true; for
 *   any other kind of nested or replacement action, this should be nil.  
 */
execNestedAction(isReplacement, isRemapping, actor, action)
{
    /* prepare the nested action */
    prepareNestedAction(isReplacement, isRemapping, action);

    /* execute the new action in the actor's sense context */
    callWithSenseContext(
        actor.isPlayerChar() ? nil : actor, sight,
        {: action.doAction(gIssuingActor, actor, nil, nil) });
}

/*
 *   Prepare a nested or replacement action for execution. 
 */
prepareNestedAction(isReplacement, isRemapping, action)
{
    /* 
     *   if the original action is an implicit command, make the new
     *   command implicit as well 
     */
    if (gAction.isImplicit)
    {
        /* 
         *   make the new action implicit, but don't describe it as a
         *   separate implicit command - it's effectively part of the
         *   original implicit command 
         */
        action.setImplicit(nil);
    }

    /* mark the new action as nested */
    action.setNested();

    /* a nested action is part of the enclosing action */
    action.setOriginalAction(gAction);

    /* if this is a remapping, mark it as such */
    if (isRemapping)
        action.setRemapped(gAction);

    /* 
     *   Set either the nested action's time or the enclosing (current)
     *   action's time to zero - we want to count only the time of one
     *   command or the other.
     *   
     *   If we're running an ordinary nested command, set the nested
     *   command's time to zero, since we want to consider it just a part
     *   of the enclosing command and thus to take no time of its own.
     *   
     *   If we're running a full replacement command, and we're replacing
     *   something other than an implied command, don't consider the
     *   enclosing command to take any time, since the enclosing command is
     *   carrying out its entire function via the replacement and thus
     *   requires no time of its own.  If we're replacing an implied
     *   command, this doesn't apply, since the implied command defers to
     *   its enclosing command for timing.  If we're replacing a command
     *   that already has zero action time, this also doesn't apply, since
     *   we're presumably replacing a command that's itself nested.  
     */
    if (isReplacement && !gAction.isImplicit && gAction.actionTime != 0)
        gAction.zeroActionTime();
    else
        action.actionTime = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Run a previously-executed command as a nested action, re-resolving
 *   all of its objects to ensure they are still valid.
 */
nestedActionAgain(action)
{
    /* reset the any cached information for the new command context */
    action.resetAction();

    /* mark the action as nested */
    action.setNested();
    action.setOriginalAction(gAction);

    /* 
     *   do not count any time for the nested action, since it's merely
     *   part of the main turn and doesn't count as a separate turn of its
     *   own 
     */
    action.actionTime = 0;

    /* execute the command */
    executeAction(gActor, nil, gIssuingActor, nil, action);
}


/* ------------------------------------------------------------------------ */
/*
 *   Run some code in a simulated Action environment.  We'll create a dummy
 *   instance of the given Action class, and set up a command transcript,
 *   then invoke the function.  This is useful for writing daemon code that
 *   needs to invoke other code that's set up to expect a normal action
 *   processing environment.  
 */
withActionEnv(actionClass, actor, func)
{
    local oldAction, oldActor;

    /* remember the old globals */
    oldAction = gAction;
    oldActor = gActor;

    try
    {
        /* set up a dummy action */
        gAction = actionClass.createInstance();

        /* use the player character as the actor */
        gActor = actor;

        /* 
         *   execute the function with a command transcript active; obtain
         *   and return the return value of the function 
         */
        return withCommandTranscript(CommandTranscript, func);
    }
    finally
    {
        /* restore globals on the way out */
        gAction = oldAction;
        gActor = oldActor;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Exit signal.  This signal indicates that we're finished with the
 *   entire command execution sequence for an action; the remainder of the
 *   command execution sequence is to be skipped for the action.  Throw
 *   this from within the command execution sequence in order to skip
 *   directly to the end-of-turn processing.  This skips everything
 *   remaining in the action, including after-action notification and the
 *   like.  This signal skips directly past the 'afterAction' phase of the
 *   command.
 *   
 *   Note that this doesn't prevent further processing of the same command
 *   if there are multiple objects involved, and it doesn't affect
 *   processing of additional commands on the same command line.  If you
 *   want to cancel further iteration of the same command for additional
 *   objects, call gAction.cancelIteration().  
 */
class ExitSignal: Exception
;

/*
 *   Exit Action signal.  This signal indicates that we're finished with
 *   the execAction portion of processing the command, but we still want
 *   to proceed with the rest of the command as normal.  This can be used
 *   when a step in the action processing wants to preempt any of the more
 *   default processing that would normally follow.  This skips directly
 *   to the 'afterAction' phase of the command.
 *   
 *   Note that this doesn't prevent further processing of the same command
 *   if there are multiple objects involved, and it doesn't affect
 *   processing of additional commands on the same command line.  If you
 *   want to cancel further iteration of the same command for additional
 *   objects, call gAction.cancelIteration().  
 */
class ExitActionSignal: Exception
;

/*
 *   Abort implicit command signal.  This exception indicates that we are
 *   aborting an implicit command without having tried to execute the
 *   command at all.  This is thrown when an implied command is to be
 *   aborted before it's even attempted, such as when verification shows
 *   the command is obviously dangerous and thus should never be attempted
 *   without the player having explicitly requesting it.  
 */
class AbortImplicitSignal: Exception
;

/* ------------------------------------------------------------------------ */
/*
 *   Action Remap signal.  This signal can be thrown only during the noun
 *   phrase resolution phase of execution, and indicates that we want to
 *   remap the action to a different action, specified in the signal.
 *   
 *   This is useful when an object is always used in a special way, so
 *   that a generic verb used with the object must be mapped to a more
 *   specific verb on the object.  For example, a game with a generic USE
 *   verb might convert USE PAINTBRUSH ON WALL to PAINT WALL WITH
 *   PAINTBRUSH by remapping the UseWith action to a PaintWith action
 *   instead.  
 */
class RemapActionSignal: Exception
    construct(action)
    {
        /* remember the new action */
        action_ = action;
    }

    /* the new action that should replace the original action */
    action_ = nil
;

/*
 *   Remap a 'verify' method for a remapped action.  This is normally
 *   invoked through the remapTo() macro.  
 */
remapVerify(oldRole, resultSoFar, remapInfo)
{
    local newAction;
    local objs;
    local idx;
    local newRole;

    /* extract new action's object list from the remapping info list */
    objs = remapInfo.sublist(2);

    /* 
     *   Create a new action object.  We only perform verification
     *   remapping during the resolution phase of the command processing,
     *   because once we've finished resolving, we'll actually replace the
     *   action with the remapped action and thus won't have to remap
     *   verification (or anything else) at that point.  So, pass true for
     *   the in-resolve flag to the action creation routine. 
     */
    newAction = remapActionCreate(true, oldRole, remapInfo);

    /* 
     *   Find the object that's given as a resolved object, rather than as
     *   a DirectObject (etc) identifier - the one given as a specific
     *   object is the one that corresponds to the original object.  
     */
    idx = objs.indexWhich({x: dataType(x) == TypeObject});

    /* get the role identifier (DirectObject, etc) for the slot position */
    newRole = newAction.getRoleFromIndex(idx);

    /* if we don't yet have a result list object, create one */
    if (resultSoFar == nil)
        resultSoFar = new VerifyResultList();

    /* if we found a remapping, verify it */
    if (idx != nil)
    {
        /* 
         *   Remember the remapped object in the result list.  Note that we
         *   do this first, before calling the remapped verification
         *   property, so that our call to the remapped verification
         *   property will overwrite this setting if it does further
         *   remapping.  We want the ultimate target object represented
         *   here, after all remappings are finished.  
         */
        resultSoFar.remapAction_ = newAction;
        resultSoFar.remapTarget_ = objs[idx];
        resultSoFar.remapRole_ = newRole;

        /* install the new action as the current action while verifying */
        local oldAction = gAction;
        gAction = newAction;

        try
        {
            /* call verification on the new object in the new role */
            return newAction.callVerifyProp(
                objs[idx], 
                newAction.getVerifyPropForRole(newRole),
                newAction.getPreCondPropForRole(newRole),
                newAction.getRemapPropForRole(newRole),
                resultSoFar, newRole);
        }
        finally
        {
            /* restore the old gAction on the way out */
            gAction = oldAction;
        }
    }
    else
    {
        /* there's no remapping, so there's nothing to verify */
        return resultSoFar;
    }
}

/*
 *   Perform a remapping to a new action.  This is normally invoked
 *   through the remapTo() macro.  
 */
remapAction(inResolve, oldRole, remapInfo)
{
    local newAction;

    /* get the new action */
    newAction = remapActionCreate(inResolve, oldRole, remapInfo);

    /* 
     *   replace the current action, using the appropriate mechanism
     *   depending on the current processing phase 
     */
    if (inResolve)
    {
        /* 
         *   we're still resolving the objects, so we must use a signal to
         *   start the resolution process over for the new action 
         */
        throw new RemapActionSignal(newAction);
    }
    else
    {
        /* 
         *   We've finished resolving everything, so we can simply use the
         *   new action as a replacement action.
         */
        execNestedAction(true, true, gActor, newAction);

        /* 
         *   the remapped action replaces the original action, so
         *   terminate the original action 
         */
        exit;
    }
}

/*
 *   Create a new action object for the given remapped action. 
 */
remapActionCreate(inResolve, oldRole, remapInfo)
{
    local newAction;
    local newObjs;
    local newActionClass;
    local objs;

    /* get the new action class and object list from the remap info */
    newActionClass = remapInfo[1];
    objs = remapInfo.sublist(2);
    
    /* 
     *   create a new instance of the replacement action, carrying forward
     *   the properties of the original (current) action 
     */
    newAction = newActionClass.createActionFrom(gAction);

    /* remember the original action we're remapping */
    newAction.setOriginalAction(gAction);

    /* set up an empty vector for the match trees for the new action */
    newObjs = new Vector(objs.length());

    /* remap according to the phase of the execution */
    if (inResolve)
    {
        /* translate the object mappings */
        foreach (local cur in objs)
        {
            /* check what we have to translate */
            if (dataType(cur) == TypeEnum)
            {
                /* 
                 *   it's an object role - if it's the special OtherObject
                 *   designator, get the other role of a two-object
                 *   command 
                 */
                if (cur == OtherObject)
                    cur = gAction.getOtherObjectRole(oldRole);
                
                /* 
                 *   get the match tree for this role from the old action
                 *   and add it to our list 
                 */
                newObjs.append(gAction.getMatchForRole(cur));
            }
            else
            {
                /* append the new ResolveInfo to the new object list */
                newObjs.append(gAction.getResolveInfo(cur, oldRole));
            }
        }

        /* set the object matches in the new action */
        newAction.setObjectMatches(newObjs.toList()...);
    }
    else
    {
        /* translate the object mappings */
        foreach (local cur in objs)
        {
            /* check what we have to translate */
            if (dataType(cur) != TypeEnum)
            {
                /* it's an explicit object - use it directly */
                newObjs.append(cur);
            }
            else
            {
                /* it's a role - translate OtherObject if needed */
                if (cur == OtherObject)
                    cur = gAction.getOtherObjectRole(oldRole);
               
                /* get the resolved object for this role */
                newObjs.append(gAction.getObjectForRole(cur));
            }
        }

        /* set the resolved objects in the new action */
        newAction.setResolvedObjects(newObjs.toList()...);
    }

    /* return the new action */
    return newAction;
}

/* ------------------------------------------------------------------------ */
/*
 *   Result message object.  This is used for verification results and
 *   main command reports, which must keep track of messages to display.  
 */
class MessageResult: object
    /* 
     *   Construct given literal message text, or alternatively a property
     *   of the current actor's verb messages object.  In either case,
     *   we'll expand the message immediately to allow the message to be
     *   displayed later with any parameters fixed at the time the message
     *   is constructed.  
     */
    construct(msg, [params])
    {
        /* if we're based on an existing object, copy its characteristics */
        if (dataType(msg) == TypeObject && msg.ofKind(MessageResult))
        {
            /* base it on the existing object */
            messageText_ = msg.messageText_;
            messageProp_ = msg.messageProp_;
            return;
        }

        /* 
         *   if the message was given as a property, remember the property
         *   for identification purposes 
         */
        if (dataType(msg) == TypeProp)
            messageProp_ = msg;

        /* 
         *   Resolve the message and store the text.  Use the action's
         *   objects (the direct object, indirect object, etc) as the
         *   sources for message overrides - this makes it easy to override
         *   messages on a per-object basis without having to rewrite the
         *   whole verify/check/action routines. 
         */
        messageText_ = resolveMessageText(gAction.getCurrentObjects(),
                                          msg, params);
    }

    /*
     *   Static method: resolve a message.  If the message is given as a
     *   property, we'll look up the message in the given source objects
     *   and in the actor's "action messages" object.  We'll return the
     *   resolved message string.  
     */
    resolveMessageText(sources, msg, params)
    {
        /*
         *   If we have more than one source object, it means that the
         *   command has more than one object slot (such as a TIAction,
         *   which has direct and indirect objects).  Rearrange the list so
         *   that the nearest caller is the first object in the list.  If
         *   one of these source objects provides an override, we generally
         *   want to get the message from the immediate caller rather than
         *   the other object.  Note that we only care about the *first*
         *   source object we find in the stack trace, because we only care
         *   about the actual message generator call; enclosing calls
         *   aren't relevant to the message priority because they don't
         *   necessarily have anything to do with the messaging.  
         */
        if (sources.length() > 1)
        {
            /* look through the stack trace for a 'self' in the source list */
            local tr = t3GetStackTrace();
            for (local i = 1, local trCnt = tr.length() ; i <= trCnt ; ++i)
            {
                /* check this 'self' */
                local s = tr[i].self_;
                local sIdx = sources.indexOf(s);
                if (sIdx != nil)
                {
                    /* 
                     *   it's a match - move this object to the head of the
                     *   list so that we give its message bindings priority
                     */
                    if (sIdx != 1)
                        sources = [s] + (sources - s);

                    /* no need to look any further */
                    break;
                }
            }
        }

        /*
         *   The message can be given either as a string or as a property
         *   of the actor's verb message object.  If it's the latter, look
         *   up the text of the property from the appropriate object.  
         */
    findTextSource:
        if (dataType(msg) == TypeProp)
        {
            local msgObj;
            
            /* 
             *   Presume that we'll read the message from the current
             *   actor's "action message object."  This is typically
             *   playerActionMessages or npcActionMessages, but it's up to
             *   the actor to specify which object we get our messages
             *   from.  
             */
            msgObj = gActor.getActionMessageObj();
            
            /*
             *   First, look up the message property in the action's
             *   objects (the direct object, indirect object, etc).  This
             *   makes it easy to override messages on a per-object basis
             *   without having to rewrite the whole verify/check/action
             *   routine.  
             */
            foreach (local cur in sources)
            {
                /* check to see if this object defines the message property */
                if (cur != nil && cur.propDefined(msg))
                {
                    local res;
                    
                    /*   
                     *   This object defines the property, so check what
                     *   we have. 
                     */
                    switch (cur.propType(msg))
                    {
                    case TypeProp:
                        /* 
                         *   It's another property, so we're being
                         *   directed back to the player action message
                         *   object.  The object does override the
                         *   message, but the override points to another
                         *   message property in the action object message
                         *   set.  Simply redirect 'msg' to point to the
                         *   new property, and use the same action message
                         *   object we already assumed we'd use.  
                         */
                        msg = cur.(msg);
                        break;

                    case TypeSString:
                        /* it's a simple string - retrieve it */
                        msg = cur.(msg);

                        /* 
                         *   since it's just a string, we're done finding
                         *   the message text - there's no need to do any
                         *   further property lookup, since we've obviously
                         *   reached the end of that particular line 
                         */
                        break findTextSource;

                    case TypeCode:
                        /*
                         *   Check the parameter count - we'll allow this
                         *   method to take the full set of parameters, or
                         *   no parameters at all.  We allow the no-param
                         *   case for convenience in cases where the method
                         *   simply wants to return a string or property ID
                         *   from a short method that doesn't need to know
                         *   the parameters; in these cases, it's
                         *   syntactically a lot nicer looking to write it
                         *   as a "prop = (expresion)" than to write the
                         *   full method-with-params syntax. 
                         */
                        if (cur.getPropParams(msg) == [0, 0, nil])
                            res = cur.(msg);
                        else
                            res = cur.(msg)(params...);

                        /*
                         *   If that returned nil, ignore it entirely and
                         *   keep scanning the remaining source objects.
                         *   The object must have decided it didn't want to
                         *   provide the message override in this case
                         *   after all. 
                         */
                        if (res == nil)
                            continue;

                        /* we didn't get nil, so use the result */
                        msg = res;

                        /* 
                         *   if we got a string, we've fully resolved the
                         *   message text, so we can stop searching for it 
                         */
                        if (dataType(msg) == TypeSString)
                            break findTextSource;

                        /*
                         *   It's not nil and it's not a string, so it must
                         *   be a property ID.  In this case, the property
                         *   ID is a property to evaluate in the normal
                         *   action message object.  Simply proceed to
                         *   evaluate the new message property as normal.  
                         */
                        break;

                    case TypeNil:
                        /* 
                         *   it's explicitly nil, which simply means to
                         *   ignore this definition; keep scanning other
                         *   source objects 
                         */
                        continue;

                    default:
                        /* 
                         *   In any other case, this must simply be the
                         *   message we're to use.  For this case, the
                         *   source of the message is this object, so
                         *   forget about the normal action message object
                         *   and instead use the current object.  Then
                         *   proceed to evaluate the message property as
                         *   normal, which will fetch it from the current
                         *   object.  
                         */
                        msgObj = cur;
                        break;
                    }

                    /* 
                     *   we found a definition, so we don't need to look
                     *   at any of the other objects involved in the
                     *   action - we just use the first override we find 
                     */
                    break;
                }
            }

            /* look up the message in the actor's message generator */
            msg = msgObj.(msg)(params...);
        }

        /* 
         *   format the string and remember the result - do the formatting
         *   immediately, because we want to make sure we expand any
         *   substitution parameters in the context of the current
         *   command, since the parameters might change (and thus alter
         *   the meaning of the message) by the time it's displayed 
         */
        msg = langMessageBuilder.generateMessage(msg);

        /* 
         *   "quote" the message text - it's fully expanded now, so
         *   there's no need to further expand anything that might by
         *   coincidence look like substitution parameters in its text 
         */
        msg = langMessageBuilder.quoteMessage(msg);

        /* return the resolved message string */
        return msg;
    }

    /* 
     *   set a new message, given the same type of information as we'd use
     *   to construct the object 
     */
    setMessage(msg, [params])
    {
        /* simply invoke the constructor to re-fill the message data */
        construct(msg, params...);
    }

    /*
     *   Display a message describing why the command isn't allowed. 
     */
    showMessage()
    {
        /* show our message string */
        say(messageText_);
    }

    /* the text of our result message */
    messageText_ = nil

    /* the message property, if we have one */
    messageProp_ = nil
;

