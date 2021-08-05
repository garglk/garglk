#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: Actions.
 *   
 *   This module defines the set of built-in library actions.  
 */

#include "adv3.h"
#include "tok.h"

/* ------------------------------------------------------------------------ */
/*
 *   Special "debug" action - this simply breaks into the debugger, if the
 *   debugger is present. 
 */
DefineIAction(Debug)
    execAction()
    {
        /* if the debugger is present, break into it */
        if (t3DebugTrace(T3DebugCheck))
            t3DebugTrace(T3DebugBreak);
        else
            "Debugger not present. ";
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Special internal action to note a change to the darkness level.  This
 *   command is invoked internally when a change to the darkness level
 *   occurs.  
 */
DefineIAction(NoteDarkness)
    execAction()
    {
        /* 
         *   if we're in the dark, note that darkness has fallen;
         *   otherwise, show the player character's room description as
         *   though the player had typed "look" 
         */
        if (gActor.isLocationLit())
        {
            /* look around */
            gActor.lookAround(true);
        }
        else
        {
            /* it is now dark */
            mainReport(&newlyDarkMsg);
        }
    }

    /* this is an internal command that takes no time */
    actionTime = 0

    /* this isn't a real action, so it's not repeatable */
    isRepeatable = nil

    /* this action doesn't do anything; don't include it in undo */
    includeInUndo = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Special "again" action.  This command repeats the previous command.
 */
DefineIAction(Again)
    /* for obvious reasons, 'again' is not itself repeatable with 'again' */
    isRepeatable = nil

    /* 
     *   the undo command itself is not undoable (but the underlying
     *   command that we repeat might be) 
     */
    includeInUndo = nil

    /* information on the most recent command */
    lastIssuingActor = nil
    lastTargetActor = nil
    lastTargetActorPhrase = nil
    lastAction = nil

    /* save the most recent command so that it can be repeated if desired */
    saveForAgain(issuingActor, targetActor, targetActorPhrase, action)
    {
        /* save the information */
        lastIssuingActor = issuingActor;
        lastTargetActor = targetActor;
        lastTargetActorPhrase = targetActorPhrase;
        lastAction = action.createClone();
    }

    /* forget the last command, so that AGAIN cannot be used */
    clearForAgain() { lastAction = nil; }

    /* 
     *   Execute the 'again' command.  This action is special enough that
     *   we override its entire action processing sequence - this is
     *   necessary in case we're repeating another special command, such
     *   as 'again', and in any case is desirable because we don't want
     *   'again' to count as a command in its own right; it's essentially
     *   just a macro that we replace with the original command. 
     */
    doAction(issuingActor, targetActor, targetActorPhrase,
             countsAsIssuerTurn)
    {
        /* if there's nothing to repeat, show an error and give up */
        if (lastAction == nil)
        {
            gLibMessages.noCommandForAgain();
            return;
        }

        /* 
         *   'again' cannot be executed with a target actor - the target
         *   actor must be the player character 
         */
        if (!targetActor.isPlayerChar)
        {
            gLibMessages.againCannotChangeActor();
            return;
        }

        /* 
         *   if the issuing actor isn't the same as the target actor, make
         *   sure the issuer can still talk to the target 
         */
        if (lastIssuingActor != lastTargetActor
            && !lastIssuingActor.canTalkTo(lastTargetActor))
        {
            /* complain that we can no longer talk to the target */
            gLibMessages.againCannotTalkToTarget(
                lastIssuingActor, lastTargetActor);
            return;
        }

        /*
         *   If the last issuing actor is different from the last target
         *   actor, then the command counts as an issuer turn, because
         *   we're effectively repeating the entire last command, including
         *   the target actor specification.  That is, after a command like
         *   "bob, go east", saying "again" is just like saying "bob, go
         *   east" again, which counts as an issuer turn.  
         */
        if (lastTargetActor != lastIssuingActor)
            countsAsIssuerTurn = true;

        /* reset any cached information for the new command context */
        lastAction.resetAction();
        
        /* repeat the action */
        lastAction.repeatAction(lastTargetActor, lastTargetActorPhrase,
                                lastIssuingActor, countsAsIssuerTurn);

        /*
         *   If the command was directed from the issuer to a different
         *   target actor, and the issuer wants to wait for the full set of
         *   issued commands to complete before getting another turn, tell
         *   the issuer to begin waiting.  
         */
        if (lastTargetActor != lastIssuingActor)
            lastIssuingActor.waitForIssuedCommand(lastTargetActor);
    }

    /* 
     *   this command itself consumes no time on the game clock (although
     *   the action we perform might) 
     */
    actionTime = 0
;

/* ------------------------------------------------------------------------ */
/*
 *   PreSaveObject - every instance of this class is notified, via its
 *   execute() method, just before we save the game.  This uses the
 *   ModuleExecObject framework, so the sequencing lists (execBeforeMe,
 *   execAfterMe) can be used to control relative ordering of execution
 *   among instances.  
 */
class PreSaveObject: ModuleExecObject
    /*
     *   Each instance must override execute() with its specific pre-save
     *   code. 
     */
;

/*
 *   PostRestoreObject - every instance of this class is notified, via its
 *   execute() method, immediately after we restore the game. 
 */
class PostRestoreObject: ModuleExecObject
    /* 
     *   note: each instance must override execute() with its post-restore
     *   code 
     */

    /*
     *   The "restore code," which is the (normally integer) value passed
     *   as the second argument to restoreGame().  The restore code gives
     *   us some idea of what triggered the restoration.  By default, we
     *   define the following restore codes:
     *   
     *   1 - the system is restoring a game as part of interpreter
     *   startup, usually because the user explicitly specified a game to
     *   restore on the interpreter command line or via a GUI shell
     *   mechanism, such as double-clicking on a saved game file from the
     *   desktop.
     *   
     *   2 - the user is explicitly restoring a game via a RESTORE command.
     *   
     *   Games and library extensions can use their own additional restore
     *   codes in their calls to restoreGame().  
     */
    restoreCode = nil
;

/*
 *   PreRestartObject - every instance of this class is notified, via its
 *   execute() method, just before we restart the game (with a RESTART
 *   command, for example). 
 */
class PreRestartObject: ModuleExecObject
    /* 
     *   Each instance must override execute() with its specific
     *   pre-restart code.  
     */
;

/*
 *   PostUndoObject - every instance of this class is notified, via its
 *   execute() method, immediately after we perform an 'undo' command. 
 */
class PostUndoObject: ModuleExecObject
    /* 
     *   Each instance must override execute() with its specific post-undo
     *   code.  
     */
;

/* ------------------------------------------------------------------------ */
/*
 *   Special "save" action.  This command saves the current game state to
 *   an external file for later restoration. 
 */
DefineAction(Save, FileOpAction)
    /* the file dialog prompt */
    filePromptMsg = (gLibMessages.getSavePrompt())

    /* we're asking for a file to save, or type t3-save */
    fileDisposition = InFileSave
    fileTypeID = FileTypeT3Save

    /* cancel message */
    showCancelMsg() { gLibMessages.saveCanceled(); }
    
    /* perform a save */
    performFileOp(fname, ack, desc:?)
    {
        /* before saving the game, notify all PreSaveObject instances */
        PreSaveObject.classExec();
        
        /* 
         *   Save the game to the given file.  If an error occurs, the
         *   save routine will throw a runtime error.  
         */
        try
        {
            /* try saving the game */
            saveGame(fname, gameMain.getSaveDesc(desc));
        }
        catch (StorageServerError sse)
        {
            /* the save failed due to a storage server problem - explain */
            gLibMessages.saveFailedOnServer(sse);

            /* done */
            return;
        }
        catch (RuntimeError err)
        {
            /* the save failed - mention the problem */
            gLibMessages.saveFailed(err);
            
            /* done */
            return;
        }
        
        /* note the successful save */
        gLibMessages.saveOkay();
    }

    /* 
     *   Saving has no effect on game state, so it's irrelevant whether or
     *   not it's undoable; but it might be confusing to say we undid a
     *   "save" command, because the player might think we deleted the
     *   saved file.  To avoid such confusion, do not include "save"
     *   commands in the undo log.  
     */
    includeInUndo = nil

    /* 
     *   Don't allow this to be repeated with AGAIN.  There's no point in
     *   repeating a SAVE immediately, as nothing will have changed in the
     *   game state to warrant saving again.  
     */
    isRepeatable = nil
;

/*
 *   Subclass of Save action that takes a literal string as part of the
 *   command.  The filename must be a literal enclosed in quotes, and the
 *   string (with the quotes) must be stored in our fname_ property by
 *   assignment of a quotedStringPhrase production in the grammar rule.  
 */
DefineAction(SaveString, SaveAction)
    execSystemAction()
    {
        /* 
         *   Perform the save, using the filename given in our fname_
         *   parameter, trimmed of quotes.  
         */
        performFileOp(fname_.getStringText(), true);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Special "restore" action.  This action restores game state previously
 *   saved with the "save" action.
 */
DefineSystemAction(Restore)
    execSystemAction()
    {
        /* ask for a file and restore it */
        askAndRestore();

        /* 
         *   regardless of what happened, abandon any additional commands
         *   on the same command line 
         */
        throw new TerminateCommandException();
    }

    /*
     *   Ask for a file and try to restore it.  Returns true on success,
     *   nil on failure.  (Failure could indicate that the user chose to
     *   cancel out of the file selector, that we couldn't find the file to
     *   restore, or that the file isn't a valid saved state file.  In any
     *   case, we show an appropriate message on failure.)  
     */
    askAndRestore()
    {
        local succ;        
        local result;
        local origElapsedTime;

        /* presume failure */
        succ = nil;

        /* note the current elapsed game time */
        origElapsedTime = realTimeManager.getElapsedTime();

        /* ask for a file */
        result = getInputFile(gLibMessages.getRestorePrompt(), InFileOpen,
                              FileTypeT3Save, 0);

        /* 
         *   restore the real-time clock, so that the time spent in the
         *   file selector dialog doesn't count against the game time 
         */
        realTimeManager.setElapsedTime(origElapsedTime);

        /* check the inputFile response */
        switch(result[1])
        {
        case InFileSuccess:
            /* 
             *   try restoring the file; use code 2 to indicate that the
             *   restoration was performed by an explicit RESTORE command 
             */
            if (performRestore(result[2], 2))
            {
                /* note that we succeeded */
                succ = true;
            }
            else
            {
                /* 
                 *   failed - in case the failed restore took some time,
                 *   restore the real-time clock, so that the file-reading
                 *   time doesn't count against the game time 
                 */
                realTimeManager.setElapsedTime(origElapsedTime);
            }

            /* done */
            break;

        case InFileFailure:
            /* advise of the failure of the prompt */
            if (result.length() > 1)
                gLibMessages.filePromptFailedMsg(result[2]);
            else
                gLibMessages.filePromptFailed();
            break;

        case InFileCancel:
            /* acknowledge the cancellation */
            gLibMessages.restoreCanceled();
            break;
        }

        /* 
         *   If we were successful, clear out the AGAIN memory.  This
         *   avoids any confusion about whether we're repeating the RESTORE
         *   command itself, the command just before RESTORE from the
         *   current session, or the last command before SAVE from the
         *   restored game. 
         */
        if (succ)
            AgainAction.clearForAgain();

        /* return the success/failure indication */
        return succ;
    }

    /*
     *   Restore a game on startup.  This can be called from mainRestore()
     *   to restore a saved game directly as part of loading the game.
     *   (Most interpreters provide a way of starting the interpreter
     *   directly with a saved game to be restored, skipping the
     *   intermediate step of running the game and using a RESTORE
     *   command.)
     *   
     *   Returns true on success, nil on failure.  On failure, the caller
     *   should simply exit the program.  On success, the caller should
     *   start the game running, usually using runGame(), after showing any
     *   desired introductory messages.  
     */
    startupRestore(fname)
    {
        /* 
         *   try restoring the game, using code 1 to indicate that this is
         *   a direct startup restore 
         */
        if (performRestore(fname, 1))
        {
            /* success - tell the caller to proceed with the restored game */
            return true;
        }
        else
        {
            /* 
             *   Failure.  We've described the problem, so ask the user
             *   what they want to do about it. 
             */
            try
            {
                /* show options and read the response */
                failedRestoreOptions();

                /* if we get here, proceed with the game */
                return true;
            }
            catch (QuittingException qe)
            {
                /* quitting - tell the caller to terminate */
                return nil;
            }
        }
    }
    

    /*
     *   Restore a file.  'code' is the restoreCode value for the
     *   PostRestoreObject notifications.  Returns true on success, nil on
     *   failure.  
     */
    performRestore(fname, code)
    {
        try
        {
            /* restore the file */
            restoreGame(fname);
        }
        catch (StorageServerError sse)
        {
            /* failed due to a storage server error - explain the problem */
            gLibMessages.restoreFailedOnServer(sse);

            /* indicate failure */
            return nil;
        }
        catch (RuntimeError err)
        {
            /* failed - check the error to see what went wrong */
            switch(err.errno_)
            {
            case 1201:
                /* not a saved state file */
                gLibMessages.restoreInvalidFile();
                break;
                
            case 1202:
                /* saved by different game or different version */
                gLibMessages.restoreInvalidMatch();
                break;
                
            case 1207:
                /* corrupted saved state file */
                gLibMessages.restoreCorruptedFile();
                break;
                
            default:
                /* some other failure */
                gLibMessages.restoreFailed(err);
                break;
            }

            /* indicate failure */
            return nil;
        }

        /* note that we've successfully restored the game */
        gLibMessages.restoreOkay();
        
        /* set the appropriate restore-action code */
        PostRestoreObject.restoreCode = code;

        /* notify all PostRestoreObject instances */
        PostRestoreObject.classExec();

        /* 
         *   look around, to refresh the player's memory of the state the
         *   game was in when saved 
         */
        "\b";
        libGlobal.playerChar.lookAround(true);

        /* indicate success */
        return true;
    }
    
    /* 
     *   There's no point in including this in undo.  If the command
     *   succeeds, it's not undoable itself, and there won't be any undo
     *   information in the newly restored state.  If the command fails, it
     *   won't make any changes to the game state, so there won't be
     *   anything to undo.  
     */
    includeInUndo = nil
;

/*
 *   Subclass of Restore action that takes a literal string as part of the
 *   command.  The filename must be a literal enclosed in quotes, and the
 *   string (with the quotes) must be stored in our fname_ property by
 *   assignment of a quotedStringPhrase production in the grammar rule.  
 */
DefineAction(RestoreString, RestoreAction)
    execSystemAction()
    {
        /* 
         *   Perform the restore, using the filename given in our fname_
         *   parameter, trimmed of quotes.  Use code 2, the same as any
         *   other explicit RESTORE command.  
         */
        performRestore(fname_.getStringText(), 2);

        /* abandon any additional commands on the same command line */
        throw new TerminateCommandException();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Restart the game from the beginning.
 */
DefineSystemAction(Restart)
    execSystemAction()
    {
        /* confirm that they really want to restart */
        gLibMessages.confirmRestart();
        if (yesOrNo())
        {
            /* 
             *   The confirmation input will have put us into
             *   start-of-command mode for sequencing purposes; force the
             *   sequencer back to mid-command mode, so we can show
             *   inter-command separation before the restart. 
             */

            /* restart the game */
            doRestartGame();
        }
        else
        {
            /* confirm that we're not really restarting */
            gLibMessages.notRestarting();
        }
    }

    /* carry out the restart action */
    doRestartGame()
    {
        /* 
         *   Show a command separator, to provide separation from any
         *   introductory text that we'll show on restarting.  Note that
         *   we probably just asked for confirmation, which means that the
         *   command sequencer will be in start-of-command mode; force it
         *   back to mid-command mode so we show inter-command separation.
         */
        commandSequencer.setCommandMode();
        "<.commandsep>";

        /* before restarting, notify anyone interested of our intentions */
        PreRestartObject.classExec();

        /* 
         *   Throw a 'restart' signal; the main entrypoint loop will catch
         *   this and actually perform the restart.
         *   
         *   Note that we *could* do the VM reset (via restartGame()) here,
         *   but there's an advantage to doing it in the main loop: we
         *   won't be in the stack context of whatever command we're
         *   performing.  If we did the restart here, it's possible that
         *   some useless objects would survive the VM reset just because
         *   they're referenced from within a caller's stack frame.  Those
         *   objects would immediately go out of scope when we get back to
         *   the main loop, but they might survive long enough to create
         *   apparent inconsistencies.  In particular, if we did a
         *   firstObj/nextObj loop, we could discover those objects and
         *   re-establish more lasting references to them, which we
         *   certainly don't want to do.  By deferring the VM reset until
         *   we get back to the main loop, we'll ensure that objects won't
         *   survive the reset just because they're on the stack
         *   momentarily here.  
         */
        throw new RestartSignal();
    }

    /* there's no point in including this in undo */
    includeInUndo = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Undo one turn. 
 */
DefineSystemAction(Undo)
    /*
     *   "Undo" is so special that we must override the entire action
     *   processing sequence.  We do this because undoing will restore the
     *   game state as of the previous savepoint, which would leave all
     *   sorts of things unsynchronized in the normal action sequence.  To
     *   avoid problems, we simply leave out any other action processing
     *   and perform the 'undo' directly.  
     */
    doAction(issuingActor, targetActor, targetActorPhrase,
             countsAsIssuerTurn)
    {
        /* 
         *   the player obviously knows about UNDO, so there's no need for
         *   a tip about it 
         */
        undoTip.makeShown();

        /* 
         *   don't allow this unless the player character is performing
         *   the command directly
         */
        if (!targetActor.isPlayerChar)
        {
            /* 
             *   tell them this command cannot be directed to another
             *   actor, and give up 
             */
            gLibMessages.systemActionToNPC();
            return;
        }

        /* perform the undo */
        performUndo(true);
    }

    /* 
     *   Perform undo.  Returns true if we were successful, nil if not.
     *   
     *   'asCommand' indicates whether or not the undo is being performed
     *   as an explicit command: if so, we'll save the UNDO command for use
     *   in AGAIN.  
     */
    performUndo(asCommand)
    {
        /* try undoing to the previous savepoint */
        if (undo())
        {
            local oldActor;
            local oldIssuer;
            local oldAction;

            /* notify all PostUndoObject instances */
            PostUndoObject.classExec();

            /* set up the globals for the command */
            oldActor = gActor;
            oldIssuer = gIssuingActor;
            oldAction = gAction;

            /* set the new globals */
            gActor = gPlayerChar;
            gIssuingActor = gPlayerChar;
            gAction = self;

            /* make sure we reset globals on the way out */
            try
            {
                /* success - mention what we did */
                gLibMessages.undoOkay(libGlobal.lastActorForUndo,
                                      libGlobal.lastCommandForUndo);
                
                /* look around, to refresh the player's memory */
                libGlobal.playerChar.lookAround(true);
            }
            finally
            {
                /* restore the parser globals to how we found them */
                gActor = oldActor;
                gIssuingActor = oldIssuer;
                gAction = oldAction;
            }
                
            /* 
             *   if this was an explicit 'undo' command, save the command
             *   to allow repeating it with 'again' 
             */
            if (asCommand)
                AgainAction.saveForAgain(gPlayerChar, gPlayerChar, nil, self);

            /* indicate success */
            return true;
        }
        else
        {
            /* no more undo information available */
            gLibMessages.undoFailed();

            /* indicate failure */
            return nil;
        }
    }

    /* 
     *   "undo" is not undoable - if we undo again after an undo, we undo
     *   the next most recent command
     */
    includeInUndo = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Save the defaults 
 */
DefineSystemAction(SaveDefaults)
    execSystemAction()
    {
        /* tell SettingsItem to save all settings */
        settingsUI.saveSettingsMsg();
    }

    /* there's no point in including this in undo */
    includeInUndo = nil
;

/*
 *   Restore defaults 
 */
DefineSystemAction(RestoreDefaults)
    execSystemAction()
    {
        /* 
         *   Tell SettingsItem to restore all settings.  This is an
         *   explicit request, so we want SettingsItem to describe what
         *   happened. 
         */
        settingsUI.restoreSettingsMsg();
    }

    /* there's no point in including this in undo */
    includeInUndo = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Quit the game. 
 */
DefineSystemAction(Quit)
    execSystemAction()
    {
        /* confirm that they really want to quit */
        gLibMessages.confirmQuit();
        if (yesOrNo())
        {
            /* carry out the termination */
            terminateGame();
        }
        else
        {
            /* show the confirmation that we're not quitting */
            gLibMessages.notTerminating();
        }
    }

    /*
     *   Carry out game termination.  This can be called when we wish to
     *   end the game without asking for any additional player
     *   confirmation.  
     */
    terminateGame()
    {
        /* acknowledge that we're quitting */
        gLibMessages.okayQuitting();
            
        /* throw a 'quitting' signal to end the game */
        throw new QuittingException;
    }

    /* there's no point in including this in undo */
    includeInUndo = nil
;

/*
 *   Pause the game.  This stops the real-time clock until the user
 *   presses a key.  Games that don't use the real-time clock will have no
 *   use for this. 
 */
DefineSystemAction(Pause)
    execSystemAction()
    {
        local elapsed;
        
        /* 
         *   remember the current elapsed game real time - when we are
         *   released from the pause, we'll restore this time 
         */
        elapsed = realTimeManager.getElapsedTime();

        /* show our prompt */
        gLibMessages.pausePrompt();

        /* keep going until we're released */
    waitLoop:
        for (;;)
        {
            /* 
             *   Wait for a key, and see what we have.  Note that we
             *   explicitly do not want to allow any real-time events to
             *   occur, so we simply wait forever without timeout. 
             */
            switch(inputKey())
            {
            case ' ':
                /* space key - end the wait */
                break waitLoop;

            case 's':
            case 'S':
                /* mention that we're saving */
                gLibMessages.pauseSaving();
                
                /* 
                 *   set the elapsed time to the time when we started, so
                 *   that the saved position reflects the time at the
                 *   start of the pause 
                 */
                realTimeManager.setElapsedTime(elapsed);

                /* save the game - go run the normal SAVE command */
                SaveAction.execSystemAction();

                /* show our prompt again */
                "<.p>";
                gLibMessages.pausePrompt();

                /* go back to wait for another key */
                break;
                
            case '[eof]':
                /* end-of-file on keyboard input - throw an error */
                "\b";
                throw new EndOfFileException();

            default:
                /* ignore other keys; just go back to wait again */
                break;
            }
        }

        /* show the released-from-pause message */
        gLibMessages.pauseEnded();

        /* 
         *   set the real-time clock to the same elapsed game time
         *   that we had when we started the pause, so that the
         *   elapsed real time of the pause itself doesn't count
         *   against the game elapsed time 
         */
        realTimeManager.setElapsedTime(elapsed);
    }
;

/*
 *   Change to VERBOSE mode. 
 */
DefineSystemAction(Verbose)
    execSystemAction()
    {
        /* set the global 'verbose' mode */
        gameMain.verboseMode.isOn = true;

        /* acknowledge it */
        gLibMessages.acknowledgeVerboseMode(true);
    }
;

/*
 *   Change to TERSE mode. 
 */
DefineSystemAction(Terse)
    execSystemAction()
    {
        /* set the global 'verbose' mode */
        gameMain.verboseMode.isOn = nil;

        /* acknowledge it */
        gLibMessages.acknowledgeVerboseMode(nil);
    }
;

/* in case the score module isn't present */
property showScore;
property showFullScore;
property scoreNotify;

/*
 *   Show the current score.
 */
DefineSystemAction(Score)
    execSystemAction()
    {
        /* show the simple score */
        if (libGlobal.scoreObj != nil)
        {
            /* show the score */
            libGlobal.scoreObj.showScore();

            /* 
             *   Mention the FULL SCORE command to the player if we haven't
             *   already.  Note that we only want to mention 
             */
            if (!mentionedFullScore)
            {
                /* explain about it */
                gLibMessages.mentionFullScore;

                /* don't mention it again */
                ScoreAction.mentionedFullScore = true;
            }
        }
        else
            gLibMessages.scoreNotPresent;
    }

    /* there's no point in including this in undo */
    includeInUndo = nil

    /* have we mentioned the FULL SCORE command yet? */
    mentionedFullScore = nil
;

/*
 *   Show the full score. 
 */
DefineSystemAction(FullScore)
    execSystemAction()
    {
        /* show the full score in response to an explicit player request */
        showFullScore();

        /* this counts as a mention of the FULL SCORE command */
        ScoreAction.mentionedFullScore = true;
    }

    /* show the full score */
    showFullScore()
    {
        /* show the full score */
        if (libGlobal.scoreObj != nil)
            libGlobal.scoreObj.showFullScore();
        else
            gLibMessages.scoreNotPresent;
    }

    /* there's no point in including this in undo */
    includeInUndo = nil
;

/*
 *   Show the NOTIFY status. 
 */
DefineSystemAction(Notify)
    execSystemAction()
    {
        /* show the current notification status */
        if (libGlobal.scoreObj != nil)
            gLibMessages.showNotifyStatus(
                libGlobal.scoreObj.scoreNotify.isOn);
        else
            gLibMessages.commandNotPresent;
    }
;

/*
 *   Turn score change notifications on. 
 */
DefineSystemAction(NotifyOn)
    execSystemAction()
    {
        /* turn notifications on, and acknowledge the status */
        if (libGlobal.scoreObj != nil)
        {
            libGlobal.scoreObj.scoreNotify.isOn = true;
            gLibMessages.acknowledgeNotifyStatus(true);
        }
        else
            gLibMessages.commandNotPresent;
    }
;

/*
 *   Turn score change notifications off. 
 */
DefineSystemAction(NotifyOff)
    execSystemAction()
    {
        /* turn notifications off, and acknowledge the status */
        if (libGlobal.scoreObj != nil)
        {
            libGlobal.scoreObj.scoreNotify.isOn = nil;
            gLibMessages.acknowledgeNotifyStatus(nil);
        }
        else
            gLibMessages.commandNotPresent;
    }
;

/*
 *   Show version information for the game and the library modules the
 *   game is using.  
 */
DefineSystemAction(Version)
    execSystemAction()
    {
        /* show the version information for each library */
        foreach (local cur in ModuleID.getModuleList())
            cur.showVersion();
    }

    /* there's no point in including this in undo */
    includeInUndo = nil
;

/*
 *   Show the credits for the game and the library modules the game
 *   includes. 
 */
DefineSystemAction(Credits)
    execSystemAction()
    {
        /* show the credits for each library */
        foreach (local cur in ModuleID.getModuleList())
            cur.showCredit();
    }

    /* there's no point in including this in undo */
    includeInUndo = nil
;

/*
 *   Show the "about" information for the game and library modules.
 */
DefineSystemAction(About)
    execSystemAction()
    {
        local anyOutput;
        
        /* watch for any output while showing module information */
        anyOutput = outputManager.curOutputStream
                    .watchForOutput(function()
        {
            /* show information for each module */
            foreach (local cur in ModuleID.getModuleList())
                cur.showAbout();
        });

        /* 
         *   if we didn't have any ABOUT information to show, display a
         *   message to this effect 
         */
        if (!anyOutput)
            gLibMessages.noAboutInfo;
    }

    /* there's no point in including this in undo */
    includeInUndo = nil
;

/*
 *   A state object that keeps track of our logging (scripting) status.
 *   This is transient, because logging is controlled through the output
 *   layer in the interpreter, which does not participate in any of the
 *   persistence mechanisms.  
 */
transient scriptStatus: object
    /*
     *   Script file name.  This is nil when logging is not in effect, and
     *   is set to the name of the scripting file when a log file is
     *   active. 
     */
    scriptFile = nil

    /* RECORD file name */
    recordFile = nil

    /* have we warned about using NOTE without logging in effect? */
    noteWithoutScriptWarning = nil
;

/* 
 *   Property: object is a web temp file.  The Web UI uses this to flag
 *   that a file we're saving to is actually a temp file that will be
 *   offered as a downloadable file to the client after the file is written
 *   and closed. 
 */
property isWebTempFile;

/*
 *   A base class for file-oriented actions, such as SCRIPT, RECORD, and
 *   REPLAY.  We provide common handling that prompts interactively for a
 *   filename; subclasses must override a few methods and properties to
 *   carry out the specific subclassed operation on the file.  
 */
DefineSystemAction(FileOp)
    /* our file dialog prompt message */
    filePromptMsg = ''

    /* the file dialog open/save type */
    fileDisposition = InFileSave

    /* the file dialog type ID */
    fileTypeID = FileTypeLog

    /* show our cancellation mesage */
    showCancelMsg = ""

    /* 
     *   Carry out our file operation.
     *   
     *   'desc' is an optional named argument giving a description string
     *   entered by the user via the Save Game dialog.  Some versions of
     *   the Save Game dialog let the user enter this additional
     *   information, which can be stored as part of the saved game
     *   metadata.  
     */
    performFileOp(fname, ack, desc:?)
    {
        /* 
         *   Each concrete action subclass must override this to carry out
         *   our operation.  This is called when the user has successfully
         *   selected a filename for the operation.  
         */
    }

    execSystemAction()
    {
        /* 
         *   ask for a file and carry out our action; since the command is
         *   being performed directly from the command line, we want an
         *   acknowledgment message on success 
         */
        setUpFileOp(true);
    }

    /* ask for a file, and carry out our operation is we get one */
    setUpFileOp(ack)
    {
        local result;
        local origElapsedTime;

        /* note the current game time */
        origElapsedTime = realTimeManager.getElapsedTime();

        /* ask for a file */
        result = getInputFile(filePromptMsg, fileDisposition, fileTypeID, 0);

        /* check the inputFile result */
        switch(result[1])
        {
        case InFileSuccess:
            /* carry out our file operation */
            if (result.length >= 3)
                performFileOp(result[2], ack, desc:result[3]);
            else
                performFileOp(result[2], ack);
            break;

        case InFileFailure:
            /* advise of the failure of the prompt */
            if (result.length() > 1)
                gLibMessages.filePromptFailedMsg(result[2]);
            else
                gLibMessages.filePromptFailed();
            break;

        case InFileCancel:
            /* acknowledge the cancellation */
            showCancelMsg();
            break;
        }

        /* 
         *   restore the original elapsed game time, so that the time spent
         *   in the file selector dialog doesn't count against the game
         *   time 
         */
        realTimeManager.setElapsedTime(origElapsedTime);
    }

    /* we can't include this in undo, as it affects external files */
    includeInUndo = nil

    /* don't allow repeating with AGAIN */
    isRepeatable = nil
;

/*
 *   Turn scripting on.  This creates a text file that contains a
 *   transcript of all commands and responses from this point forward.
 */
DefineAction(Script, FileOpAction)
    /* our file dialog parameters - ask for a log file to save */
    filePromptMsg = (gLibMessages.getScriptingPrompt())
    fileTypeID = FileTypeLog
    fileDisposition = InFileSave

    /* show our cancellation mesasge */
    showCancelMsg() { gLibMessages.scriptingCanceled(); }

    /* 
     *   set up scripting - this can be used to set up scripting
     *   programmatically, in the course of carrying out another action 
     */
    setUpScripting(ack) { setUpFileOp(ack); }

    /* turn on scripting to the given file */
    performFileOp(fname, ack)
    {
        /* turn on logging */
        local ok = nil, exc = nil;
        try
        {
            ok = aioSetLogFile(fname, LogTypeTranscript);
        }
        catch (Exception e)
        {
            exc = e;
        }
        if (ok)
        {
            /* remember that scripting is in effect */
            scriptStatus.scriptFile = fname;

            /* 
             *   forget any past warning that we've issued about NOTE
             *   without a script in effect; the next time scripting isn't
             *   active, we'll want to issue a new warning, since they
             *   might not be aware at that point that the scripting we're
             *   starting now has ended 
             */
            scriptStatus.noteWithoutScriptWarning = nil;

            /* note that logging is active, if acknowledgment is desired */
            if (ack)
            {
                if (fname.isWebTempFile)
                    gLibMessages.scriptingOkayWebTemp();
                else
                    gLibMessages.scriptingOkay();
            }
        }
        else
        {
            /* scripting is no longer in effect */
            scriptStatus.scriptFile = nil;

            /* show an error, if acknowledgment is desired */
            if (ack)
            {
                if (exc != nil)
                    gLibMessages.scriptingFailedException(exc);
                else
                    gLibMessages.scriptingFailed;
            }
        }
    }
;

/*
 *   Subclass of Script action taking a quoted string as part of the
 *   command syntax.  The grammar rule must set our fname_ property to a
 *   quotedStringPhrase subproduction. 
 */
DefineAction(ScriptString, ScriptAction)
    execSystemAction()
    {
        /* if there's a filename, we don't need to prompt */
        if (fname_ != nil)
        {
            /* set up scripting to the filename specified in the command */
            performFileOp(fname_.getStringText(), true);
        }
        else
        {
            /* there's no filename, so prompt as usual */
            inherited();
        }
    }
;

/*
 *   Turn scripting off.  This stops recording the game transcript started
 *   with the most recent SCRIPT command. 
 */
DefineSystemAction(ScriptOff)
    execSystemAction()
    {
        /* turn off scripting */
        turnOffScripting(true);
    }

    /* turn off scripting */
    turnOffScripting(ack)
    {
        /* if we're not in a script file, ignore it */
        if (scriptStatus.scriptFile == nil)
        {
            gLibMessages.scriptOffIgnored();
            return;
        }

        /* cancel scripting in the interpreter's output layer */
        aioSetLogFile(nil, LogTypeTranscript);

        /* remember that scripting is no longer in effect */
        scriptStatus.scriptFile = nil;

        /* acknowledge the change, if desired */
        if (ack)
            gLibMessages.scriptOffOkay();
    }

    /* we can't include this in undo, as it affects external files */
    includeInUndo = nil
;

/*
 *   RECORD - this is similar to SCRIPT, but stores a file containing only
 *   the command input, not the output. 
 */
DefineAction(Record, FileOpAction)
    /* our file dialog parameters - ask for a log file to save */
    filePromptMsg = (gLibMessages.getRecordingPrompt())
    fileTypeID = FileTypeCmd
    fileDisposition = InFileSave

    /* show our cancellation mesasge */
    showCancelMsg() { gLibMessages.recordingCanceled(); }

    /* 
     *   set up recording - this can be used to set up scripting
     *   programmatically, in the course of carrying out another action 
     */
    setUpRecording(ack) { setUpFileOp(ack); }

    /* turn on recording to the given file */
    performFileOp(fname, ack)
    {
        /* turn on command logging */
        local ok = nil, exc = nil;
        try
        {
            ok = aioSetLogFile(fname, logFileType);
        }
        catch (Exception e)
        {
            exc = e;
        }
        if (ok)
        {
            /* remember that recording is in effect */
            scriptStatus.recordFile = fname;

            /* note that logging is active, if acknowledgment is desired */
            if (ack)
                gLibMessages.recordingOkay();
        }
        else
        {
            /* recording failed */
            scriptStatus.recordFile = nil;

            /* show an error if acknowledgment is desired */
            if (ack)
            {
                if (exc != nil)
                    gLibMessages.recordingFailedException(exc);
                else
                    gLibMessages.recordingFailed();
            }
        }
    }

    /* the log file type - by default, we open a regular command log */
    logFileType = LogTypeCommand
;

/* subclass of Record action that sets up an event script recording */
DefineAction(RecordEvents, RecordAction)
    logFileType = LogTypeScript
;

/* subclass of Record action taking a quoted string for the filename */
DefineAction(RecordString, RecordAction)
    execSystemAction()
    {
        /* set up scripting to the filename specified in the command */
        performFileOp(fname_.getStringText(), true);
    }
;

/* subclass of RecordString action that sets up an event script recording */
DefineAction(RecordEventsString, RecordStringAction)
    logFileType = LogTypeScript
;

/*
 *   Turn command recording off.  This stops recording the command log
 *   started with the most recent RECORD command.  
 */
DefineSystemAction(RecordOff)
    execSystemAction()
    {
        /* turn off recording */
        turnOffRecording(true);
    }

    /* turn off recording */
    turnOffRecording(ack)
    {
        /* if we're not recording anything, ignore it */
        if (scriptStatus.recordFile == nil)
        {
            gLibMessages.recordOffIgnored();
            return;
        }

        /* cancel recording in the interpreter's output layer */
        aioSetLogFile(nil, LogTypeCommand);

        /* remember that recording is no longer in effect */
        scriptStatus.recordFile = nil;

        /* acknowledge the change, if desired */
        if (ack)
            gLibMessages.recordOffOkay();
    }

    /* we can't include this in undo, as it affects external files */
    includeInUndo = nil
;

/*
 *   REPLAY - play back a command log previously recorded. 
 */
DefineAction(Replay, FileOpAction)
    /* our file dialog parameters - ask for a log file to save */
    filePromptMsg = (gLibMessages.getReplayPrompt())
    fileTypeID = FileTypeCmd
    fileDisposition = InFileOpen

    /* show our cancellation mesasge */
    showCancelMsg() { gLibMessages.replayCanceled(); }

    /* script flags passed to setScriptFile */
    scriptOptionFlags = 0

    /* replay the given file */
    performFileOp(fname, ack)
    {
        /* 
         *   Note that we're reading from the script file if desired.  Do
         *   this before opening the script, so that we display the
         *   acknowledgment even if we're in 'quiet' mode. 
         */
        if (ack)
            gLibMessages.inputScriptOkay(
                fname.ofKind(TemporaryFile) ? fname.getFilename() : fname);

        /* activate the script file */
        local ok = nil, exc = nil;
        try
        {
            ok = setScriptFile(fname, scriptOptionFlags);
        }
        catch (Exception e)
        {
            exc = e;
        }
        if (!ok)
        {
            if (exc != nil)
                gLibMessages.inputScriptFailed(exc);
            else
                gLibMessages.inputScriptFailed();
        }
    }
;

/* subclass of Replay action taking a quoted string for the filename */
DefineAction(ReplayString, ReplayAction)
    execSystemAction()
    {
        /* 
         *   if there's a string, use the string as the filename;
         *   otherwise, inherit the default handling to ask for a filename 
         */
        if (fname_ != nil)
        {
            /* set up scripting to the filename specified in the command */
            performFileOp(fname_.getStringText(), true);
        }
        else
        {
            /* inherit the default handling to ask for a filename */
            inherited();
        }
    }
;


/* in case the footnote module is not present */
property showFootnote;

/*
 *   Footnote - this requires a numeric argument parsed via the
 *   numberPhrase production and assigned to the numMatch property.  
 */
DefineSystemAction(Footnote)
    execSystemAction()
    {
        /* ask the Footnote class to do the work */
        if (libGlobal.footnoteClass != nil)
            libGlobal.footnoteClass.showFootnote(numMatch.getval());
        else
            gLibMessages.commandNotPresent;
    }

    /* there's no point in including this in undo */
    includeInUndo = nil
;

property footnoteSettings;

/* base class for FOOTNOTES xxx commands */
DefineSystemAction(Footnotes)
    execSystemAction()
    {
        if (libGlobal.footnoteClass != nil)
        {
            /* set my footnote status in the global setting */
            libGlobal.footnoteClass.footnoteSettings.showFootnotes =
                showFootnotes;

            /* acknowledge it */
            gLibMessages.acknowledgeFootnoteStatus(showFootnotes);
        }
        else
            gLibMessages.commandNotPresent;
    }

    /* 
     *   the footnote status I set when this command is activated - this
     *   must be overridden by each subclass 
     */
    showFootnotes = nil
;

DefineAction(FootnotesFull, FootnotesAction)
    showFootnotes = FootnotesFull
;

DefineAction(FootnotesMedium, FootnotesAction)
    showFootnotes = FootnotesMedium
;

DefineAction(FootnotesOff, FootnotesAction)
    showFootnotes = FootnotesOff
;

DefineSystemAction(FootnotesStatus)
    execSystemAction()
    {
        /* show the current status */
        if (libGlobal.footnoteClass != nil)
            gLibMessages.showFootnoteStatus(libGlobal.footnoteClass.
                                            footnoteSettings.showFootnotes);
        else
            gLibMessages.commandNotPresent;
    }

    /* there's no point in including this in undo */
    includeInUndo = nil
;

DefineIAction(Inventory)
    execAction()
    {
        /* show the actor's inventory in the current mode */
        gActor.showInventory(inventoryMode == InventoryTall);
    }

    /* current inventory mode - start in 'wide' mode by default */
    inventoryMode = InventoryWide;
;

DefineIAction(InventoryTall)
    execAction()
    {
        /* set inventory mode to 'tall' */
        InventoryAction.inventoryMode = InventoryTall;

        /* run the inventory action */
        InventoryAction.checkAction();
        InventoryAction.execAction();
    }
;

DefineIAction(InventoryWide)
    execAction()
    {
        /* set inventory mode to 'wide' */
        InventoryAction.inventoryMode = InventoryWide;

        /* run the inventory action */
        InventoryAction.checkAction();
        InventoryAction.execAction();
    }
;

DefineIAction(Wait)
    execAction()
    {
        /* just show the "time passes" message */
        defaultReport(&timePassesMsg);
    }
;

DefineIAction(Look)
    execAction()
    {
        /* show the actor's current location in verbose mode */
        gActor.lookAround(true);
    }
;

DefineIAction(Sleep)
    execAction()
    {
        /* let the actor handle it */
        gActor.goToSleep();
    }
;

DefineTAction(Take)
    /* this is a basic inventory-management verb, so allow ALL with it */
    actionAllowsAll = true

    /* get the ALL list for the direct object */
    getAllDobj(actor, scopeList)
    {
        local locList;
        local dropLoc;
        local actorLoc;
        
        /*
         *   Include all of the objects that are directly in the actor's
         *   immediate container, the container's container, and so on out
         *   to the "drop destination" location (which is where things go
         *   when we DROP them, and is meant to represent the nearest
         *   platform-like or floor-like container).  Also include anything
         *   that's directly in anything fixed in place within one of these
         *   containers.  Don't include anything that actually contains the
         *   actor, since we normally can't pick up something we're inside.
         *   
         *   Start by getting the actor's immediate location and drop
         *   destination location.  
         */
        actorLoc = actor.location;
        dropLoc = actor.getDropDestination(nil, nil);

        /* 
         *   create a vector to hold the location list, and start it off
         *   with the drop location 
         */
        locList = new Vector(10);
        locList.append(dropLoc);

        /* now work up the location list until we hit the drop location */
        for (local cur = actorLoc ; cur != nil && cur != dropLoc ;
             cur = cur.location)
        {
            /* add this container to the list */
            locList.append(cur);
        }

        /* 
         *   now generate the subset of in-scope objects that are directly
         *   in any of these locations (or directly in items fixed in place
         *   within any of these locations), and return the result 
         */
        return scopeList.subset(
            {x: (locList.indexWhich(
                {loc: x.isDirectlyIn(loc) || x.isInFixedIn(loc)}) != nil
                 && !actor.isIn(x)) });
    }
;

DefineTIAction(TakeFrom)
    /* this is a basic inventory-management verb, so allow ALL with it */
    actionAllowsAll = true

    /* get the ALL list for the direct object */
    getAllDobj(actor, scopeList)
    {
        /* ask the indirect object for the list */
        return getIobj() == nil
            ? []
            : getIobj().getAllForTakeFrom(scopeList);
    }
;

DefineTAction(Remove)
;

DefineTAction(Drop)
    /* this is a basic inventory-management verb, so allow ALL with it */
    actionAllowsAll = true

    /* get the ALL list for the direct object */
    getAllDobj(actor, scopeList)
    {
        /* include only objects directly held by the actor */
        return scopeList.subset({x: x.isDirectlyIn(actor)});
    }
;

DefineTAction(Examine)
;

DefineTAction(Read)
;

DefineTAction(LookIn)
;

DefineTAction(Search)
;

DefineTAction(LookUnder)
;

DefineTAction(LookBehind)
;

DefineTAction(LookThrough)
;

DefineTAction(Feel)
;

DefineTAction(Taste)
;

DefineTAction(Smell)
;

DefineTAction(ListenTo)
;

/*
 *   Base class for undirected sensing, such as "listen" or "smell" with no
 *   object.  We'll scan for things that have a presence in the
 *   corresponding sense and describe each one.  
 */
DefineIAction(SenseImplicit)
    /* the sense in which I operate */
    mySense = nil

    /* the object property to display this sense's description */
    descProp = nil
    
    /* the default message to display if we find nothing specific to sense */
    defaultMsgProp = nil

    /* the Lister we use to show the items */
    resultLister = nil

    /* execute the action */
    execAction()
    {
        local senseTab;
        local presenceList;
            
        /* get a list of everything in range of this sense for the actor */
        senseTab = gActor.senseInfoTable(mySense);

        /* get a list of everything with a presence in this sense */
        presenceList = senseInfoTableSubset(senseTab,
            {obj, info: obj.(mySense.presenceProp)});

        /* 
         *   if there's anything in the list, show it; otherwise, show a
         *   default report 
         */
        if (presenceList.length() != 0)
        {
            /* show the list using our lister */
            resultLister.showList(gActor, nil, presenceList, 0, 0,
                                  senseTab, nil);
        }
        else
        {
            /* there's nothing to show - say so */
            defaultReport(defaultMsgProp);
        }
    }
;

DefineAction(SmellImplicit, SenseImplicitAction)
    mySense = smell
    descProp = &smellDesc
    defaultMsgProp = &nothingToSmellMsg
    resultLister = smellActionLister
;

DefineAction(ListenImplicit, SenseImplicitAction)
    mySense = sound
    descProp = &soundDesc
    defaultMsgProp = &nothingToHearMsg
    resultLister = listenActionLister
;

DefineTIAction(PutIn)
    /* this is a basic inventory-management verb, so allow ALL with it */
    actionAllowsAll = true

    /* get the ALL list for the direct object */
    getAllDobj(actor, scopeList)
    {
        local loc;
        local iobj = nil;
        local iobjIdent = nil;

        /* get the actor's location */
        loc = actor.location;

        /* if we have an iobj list, retrieve its first element */
        if (iobjList_ != nil && iobjList_.length() > 0)
        {
            iobj = iobjList_[1].obj_;
            iobjIdent = iobj.getIdentityObject();
        }
        
        /*
         *   Include objects that are directly in the actor's location, or
         *   within fixed items in the actor's location, or directly in the
         *   actor's inventory.
         *   
         *   Exclude the indirect object and its "identity" object (since
         *   we obviously can't put the indirect object in itself), and
         *   exclude everything already directly in the indirect object.  
         */
        return scopeList.subset({x:
                                (x.isDirectlyIn(loc)
                                 || x.isInFixedIn(loc)
                                 || x.isDirectlyIn(actor))
                                && x != iobj
                                && x != iobjIdent
                                && !x.isDirectlyIn(iobj)});
    }
;

DefineTIAction(PutOn)
    /* this is a basic inventory-management verb, so allow ALL with it */
    actionAllowsAll = true

    /* get the ALL list for the direct object */
    getAllDobj(actor, scopeList)
    {
        /* use the same strategy that we do in PutIn */
        local loc = actor.location;
        return scopeList.subset({x:
                                (x.isDirectlyIn(loc)
                                 || x.isInFixedIn(loc)
                                 || x.isDirectlyIn(actor))
                                && x != getIobj()
                                && !x.isDirectlyIn(getIobj())});
    }
;

DefineTIAction(PutUnder)
;

DefineTIAction(PutBehind)
;

DefineTAction(Wear)
;

DefineTAction(Doff)
;

DefineConvTopicTAction(AskFor, IndirectObject)
;

DefineConvTopicTAction(AskAbout, IndirectObject)
;

DefineConvTopicTAction(TellAbout, IndirectObject)
    /*
     *   TELL ABOUT is a conversational address, as opposed to an order,
     *   if the direct object of the action is the same as the issuer: in
     *   this case, the command has the form <actor>, TELL ME ABOUT
     *   <topic>, which has exactly the same meaning as ASK <actor> ABOUT
     *   <topic>. 
     */
    isConversational(issuer)
    {
        local dobj;
        
        /* 
         *   if the resolved direct object matches the issuer, it's
         *   conversational 
         */
        dobj = getResolvedDobjList();
        return (dobj.length() == 1 && dobj[1] == issuer);
    }
;

/*
 *   AskVague and TellVague are for syntactically incorrect phrasings that
 *   a player might accidentally type, especially in conjunction with a
 *   past SpecialTopic prompt; in English, for example, we define these as
 *   ASK <actor> <topic> and TELL <actor> <topic>.  These are used only to
 *   provide more helpful error messages.  
 */
DefineTopicTAction(AskVague, IndirectObject)
;
DefineTopicTAction(TellVague, IndirectObject)
;

DefineConvIAction(Hello)
    execAction()
    {
        /* the issuing actor is saying hello to the target actor */
        gIssuingActor.sayHello(gActor);
    }
;

DefineConvIAction(Goodbye)
    execAction()
    {
        /* the issuing actor is saying goodbye to the target actor */
        gIssuingActor.sayGoodbye(gActor);
    }
;

DefineConvIAction(Yes)
    execAction()
    {
        /* the issuing actor is saying yes to the target actor */
        gIssuingActor.sayYes(gActor);
    }
;

DefineConvIAction(No)
    execAction()
    {
        /* the issuing actor is saying no to the target actor */
        gIssuingActor.sayNo(gActor);
    }
;

/*
 *   Invoke the active SpecialTopic.  This isn't a real command - the
 *   player will never actually type this; rather, it's a pseudo-command
 *   that we send to ourselves from a string pre-parser when we recognize
 *   input that matches a SpecialTopic's custom command syntax.
 *   
 *   Note that we actually define the syntax for this command right here
 *   in the language-independent library, because this isn't a real
 *   command.  The user never needs to type this command, since it's
 *   something we generate internally.  The only important language issue
 *   is that we use a command keyword that no language will ever want to
 *   use for a real command, so we intentionally use some near-English
 *   gibberish.  
 */
DefineLiteralAction(SpecialTopic)
    execAction()
    {
        /* 
         *   the issuing actor is saying the current special topic to the
         *   actor's current interlocutor 
         */
        gIssuingActor.saySpecialTopic();
    }

    /* 
     *   Repeat the action, for an AGAIN command.  We need to make sure
     *   the special text interpretation we gave to the command still
     *   holds; if not, reparse the original text and try that.  
     */
    repeatAction(lastTargetActor, lastTargetActorPhrase,
                 lastIssuingActor, countsAsIssuerTurn)
    {
        local cmd;

        /* get the original text the player entered */
        cmd = getEnteredText();
        
        /* 
         *   try running this through the special topic pre-parser again,
         *   to see if it still has the special meaning 
         */
        if (specialTopicPreParser.doParsing(cmd, rmcCommand)
            .startsWith('xspcltopic '))
        {
            /* 
             *   it still has the special meaning, so simply execute as we
             *   normally would, by inheriting the standard Action
             *   handling 
             */
            inherited(lastTargetActor, lastTargetActorPhrase,
                      lastIssuingActor, countsAsIssuerTurn);
        }
        else
        {
            /*
             *   The command no longer has the special meaning it did on
             *   the last command, so we can't repeat this command.  
             */
            gLibMessages.againNotPossible(lastIssuingActor);
        }
    }

    /*
     *   Get the original player-entered text.  This is our literal
     *   phrase, with the embedded-quote encoding decoded.
     */
    getEnteredText() { return decodeOrig(getLiteral()); }

    /* 
     *   encode the original text for our literal phrase: turn double
     *   quotes into '%q' sequences, and turn percent signs into '%%'
     *   sequences 
     */
    encodeOrig(txt)
    {
        /* replace '%' with '%%', and double quotes with '%q' */
        return txt.findReplace(['%', '"'], ['%%', '%q']);
    }

    /* decode our encoding */
    decodeOrig(txt)
    {
        /* replace '%%' with '%', and '%q' with '"' */
        return txt.findReplace(['%%', '%q'], ['%', '"']);
    }
;

grammar predicate(SpecialTopic):
    'xspcltopic' literalPhrase->literalMatch
    : SpecialTopicAction

    /*
     *   Use the text of the command as originally typed by the player as
     *   our apparent original text.  
     */
    getOrigText() { return getEnteredText(); }
;

/* in case they try typing just 'xspcltopic' */
grammar predicate(EmptySpecialTopic):
    'xspcltopic' : IAction

    /* just act like we don't understand this command */
    execAction() { gLibMessages.commandNotPresent; }
;

DefineTAction(Kiss)
;

DefineIAction(Yell)
    execAction()
    {
        /* yelling generally has no effect; issue a default response */
        mainReport(&okayYellMsg);
    }
;

DefineTAction(TalkTo)
;

DefineSystemAction(Topics)
    execSystemAction()
    {
        /* check to see if any suggestions are defined in the entire game */
        if (firstObj(SuggestedTopic, ObjInstances) != nil)
        {
            /* we have topics - let the actor handle it */
            gActor.suggestTopics(true);
        }
        else
        {
            /* there are no topics at all, so this command isn't used */
            gLibMessages.commandNotPresent;
        }
    }

    /* don't include this in undo */
    includeInUndo = nil
;

DefineTIAction(GiveTo)
    getDefaultIobj(np, resolver)
    {
        /* check the actor for a current interlocutor */
        local obj = resolver.getTargetActor().getCurrentInterlocutor();
        if (obj != nil)
            return [new ResolveInfo(obj, 0, np)];
        else
            return inherited(np, resolver);
    }
;

/*
 *   Define a global remapping to transform commands of the form "X, GIVE
 *   ME Y" to the format "ME, ASK X FOR Y".  This makes it easier to write
 *   the code to handle these sorts of exchanges, since it means you only
 *   have to write it in the ASK FOR handler.  
 */
giveMeToAskFor: GlobalRemapping
    /*
     *   Remap a command, if applicable.  We look for commands of the form
     *   "X, GIVE ME Y": we look for a GiveTo action whose indirect object
     *   is the same as the issuing actor.  When we find this form of
     *   command, we rewrite it to "ME, ASK X FOR Y".  
     */
    getRemapping(issuingActor, targetActor, action)
    {
        /* 
         *   if it's of the form "X, GIVE Y TO Z", where Z is the issuing
         *   actor (generally ME, but it could conceivably be someone
         *   else), transform it into "Z, ASK X FOR Y".  
         */
        if (action.ofKind(GiveToAction)
            && action.canIobjResolveTo(issuingActor))
        {
            /* create the ASK FOR action */
            local newAction = AskForAction.createActionInstance();

            /* remember the original version of the action */
            newAction.setOriginalAction(action);

            /*
             *   Changing the phrasing from "X, GIVE Y TO Z" to "Z, ASK X
             *   FOR Y" will change the target actor from X in the old
             *   version to Z in the new version.  In the original format,
             *   the pronouns "you", "your", and "yours" implicitly refers
             *   to Z ("Bob, give me your book" implies "bob's book").  The
             *   rewrite will change that, though - assuming that Z is a
             *   second-person actor, "you" will by default refer to Z in
             *   the rewrite.  In order to preserve the original meaning,
             *   we have to override "you" in the rewrite so that it
             *   continues to refer to "X", which we can do using a pronoun
             *   override in the new action.  
             */
            newAction.setPronounOverride(PronounYou, targetActor);

            /* 
             *   The direct object - the person we're asking - is the
             *   original target actor ("bob" in "bob, give me x").  Since
             *   this is a specific object, we need to wrap it in a
             *   PreResolvedProd.
             */
            local dobj = new PreResolvedProd(targetActor);

            /*
             *   The thing we're asking for is the original direct object.
             *   ASK FOR takes a topic phrase for its indirect object,
             *   whereas GIVE TO takes a regular noun phrase.  The two
             *   aren't quite identical syntactically, so we'll do better
             *   if we re-parse the original dobj noun phrase as a topic
             *   phrase.  Fortunately, this is easy...
             */
            local iobj = newAction.reparseMatchAsTopic(
                action.dobjMatch, issuingActor, issuingActor);

            /* set the object match trees */
            newAction.setObjectMatches(dobj, iobj);
            
            /* 
             *   Return the new command, addressing the *issuing* actor
             *   this time around. 
             */
            return [issuingActor, newAction];
        }

        /* it's not ours */
        return nil;
    }
;


DefineTIAction(ShowTo)
    getDefaultIobj(np, resolver)
    {
        /* check the actor for a current interlocutor */
        local obj = resolver.getTargetActor().getCurrentInterlocutor();
        if (obj != nil)
            return [new ResolveInfo(obj, 0, np)];
        else
            return inherited(np, resolver);
    }
;

DefineTAction(Follow)
    /*
     *   For resolving our direct object, we want to include in the scope
     *   any item that isn't present but which the actor saw departing the
     *   present location.  
     */
    initResolver(issuingActor, targetActor)
    {
        /* inherit the base resolver initialization */
        inherited(issuingActor, targetActor);

        /* 
         *   add to the scope all of the actor's followable objects -
         *   these are the objects which the actor has witnessed leaving
         *   the actor's present location 
         */
        scope_ = scope_.appendUnique(targetActor.getFollowables());
    }
;

DefineTAction(Attack)
;

DefineTIAction(AttackWith)
    /* 
     *   for the indirect object, limit 'all' and defaults to the items in
     *   inventory 
     */
    getAllIobj(actor, scopeList)
    {
        return scopeList.subset({x: x.isIn(actor)});
    }
;

DefineTAction(Throw)
;

DefineTAction(ThrowDir)
    /* get the direction of the throwing (as a Direction object) */
    getDirection() { return dirMatch.dir; }
;

DefineTIAction(ThrowAt)
;

DefineTIAction(ThrowTo)
;

DefineTAction(Dig)
;

DefineTIAction(DigWith)
    /* limit 'all' for the indirect object to items in inventory */
    getAllIobj(actor, scopeList)
    {
        return scopeList.subset({x: x.isIn(actor)});
    }
;

DefineIAction(Jump)
    preCond = [actorStanding]
    execAction()
    {
        /* show the default report for jumping in place */
        mainReport(&okayJumpMsg);
    }
;

DefineTAction(JumpOver)
;

DefineTAction(JumpOff)
;

DefineIAction(JumpOffI)
    execAction()
    {
        mainReport(&cannotJumpOffHereMsg);
    }
;

DefineTAction(Push)
;

DefineTAction(Pull)
;

DefineTAction(Move)
;

DefineTIAction(MoveWith)
    /* limit 'all' for the indirect object to items in inventory */
    getAllIobj(actor, scopeList)
    {
        return scopeList.subset({x: x.isIn(actor)});
    }
;

DefineTIAction(MoveTo)
;

DefineTAction(Turn)
;

DefineTIAction(TurnWith)
    /* limit 'all' for the indirect object to items in inventory */
    getAllIobj(actor, scopeList)
    {
        return scopeList.subset({x: x.isIn(actor)});
    }
;

DefineLiteralTAction(TurnTo, IndirectObject)
;

DefineTAction(Set)
;

DefineLiteralTAction(SetTo, IndirectObject)
;

DefineTAction(TypeOn)
;

DefineLiteralTAction(TypeLiteralOn, DirectObject)
;

DefineLiteralTAction(EnterOn, DirectObject)
;

DefineTAction(Consult)
;

DefineTopicTAction(ConsultAbout, IndirectObject)
    getDefaultDobj(np, resolver)
    {
        /* 
         *   if the actor has consulted something before, and that object
         *   is still visible, use it as the default for this consultation 
         */
        local actor = resolver.getTargetActor();
        local obj = actor.lastConsulted;
        if (obj != nil && actor.canSee(obj))
            return [new ResolveInfo(obj, DefaultObject, np)];
        else
            return inherited(np, resolver);
    }

    /*
     *   Filter the topic phrase resolution.  If we know our direct object,
     *   and it's a Consultable, refer the resolution to the Consultable.  
     */
    filterTopic(lst, np, resolver)
    {
        local dobj;
        
        /* check the direct object */
        if (dobjList_ != nil
            && dobjList_.length() == 1
            && (dobj = dobjList_[1].obj_).ofKind(Consultable))
        {
            /* 
             *   we have a Consultable direct object - let it handle the
             *   topic phrase resolution 
             */
            return dobj.resolveConsultTopic(lst, topicMatch, resolver);
        }
        else
        {
            /* otherwise, use the default handling */
            return inherited(lst, np, resolver);
        }
    }
;

DefineTAction(Switch)
;

DefineTAction(Flip)
;

DefineTAction(TurnOn)
;

DefineTAction(TurnOff)
;

DefineTAction(Light)
;

DefineTAction(Burn)
;

DefineTIAction(BurnWith)
    /* limit 'all' for the indirect object to items in inventory */
    getAllIobj(actor, scopeList)
    {
        return scopeList.subset({x: x.isIn(actor)});
    }

    /* resolve the direct object first */
    resolveFirst = DirectObject
;

DefineTAction(Extinguish)
;

DefineTIAction(AttachTo)
;

DefineTIAction(DetachFrom)
;

DefineTAction(Detach)
;

DefineTAction(Break)
;

DefineTAction(Cut)
;

DefineTIAction(CutWith)
;

DefineTAction(Climb)
;

DefineTAction(ClimbUp)
;

DefineTAction(ClimbDown)
;

DefineTAction(Open)
;

DefineTAction(Close)
;

DefineTAction(Lock)
;

DefineTAction(Unlock)
;

DefineTIAction(LockWith)
    /* 
     *   Resolve the direct object (the lock) first, so that we know what
     *   we're trying to unlock when we're verifying the key.  This allows
     *   us to (optionally) boost the likelihood of a known good key for
     *   disambiguation. 
     */
    resolveFirst = DirectObject

    /* limit 'all' for the indirect object to items in inventory */
    getAllIobj(actor, scopeList)
    {
        return scopeList.subset({x: x.isIn(actor)});
    }
;

DefineTIAction(UnlockWith)
    /* resolve the direct object first, for the same reason as in LockWith */
    resolveFirst = DirectObject

    /* limit 'all' for the indirect object to items in inventory */
    getAllIobj(actor, scopeList)
    {
        return scopeList.subset({x: x.isIn(actor)});
    }
;

DefineTAction(Eat)
;

DefineTAction(Drink)
;

DefineTAction(Pour)
;

DefineTIAction(PourInto)
;

DefineTIAction(PourOnto)
;

DefineTAction(Clean)
;

DefineTIAction(CleanWith)
    /* limit 'all' for the indirect object to items in inventory */
    getAllIobj(actor, scopeList)
    {
        return scopeList.subset({x: x.isIn(actor)});
    }
;

DefineIAction(Sit)
    execAction()
    {
        /* 
         *   if the actor is already sitting, just say so; otherwise, ask
         *   what they want to sit on 
         */
        if (gActor.posture == sitting)
            reportFailure(&alreadySittingMsg);
        else
            askForDobj(SitOn);
    }
;

DefineTAction(SitOn)
;

DefineIAction(Lie)
    execAction()
    {
        /* 
         *   if the actor is already lying down, just say so; otherwise,
         *   ask what they want to lie on 
         */
        if (gActor.posture == lying)
            reportFailure(&alreadyLyingMsg);
        else
            askForDobj(LieOn);
    }
;

DefineTAction(LieOn)
;

DefineTAction(StandOn)
;

DefineIAction(Stand)
    execAction()
    {
        /* let the actor handle it */
        gActor.standUp();
    }
;

DefineTAction(Board)
;

DefineTAction(GetOutOf)
    getAllDobj(actor, scopeList)
    {
        /* 'all' for 'get out of' is the actor's immediate container */
        return scopeList.subset({x: actor.isDirectlyIn(x)});
    }
;

DefineTAction(GetOffOf)
    getAllDobj(actor, scopeList)
    {
        /* 'all' for 'get off of' is the actor's immediate container */
        return scopeList.subset({x: actor.isDirectlyIn(x)});
    }
;

DefineIAction(GetOut)
    execAction()
    {
        /* let the actor handle it */
        gActor.disembark();
    }
;

DefineTAction(Fasten)
;

DefineTIAction(FastenTo)
;

DefineTAction(Unfasten)
;

DefineTIAction(UnfastenFrom)
;

DefineTAction(PlugIn)
;

DefineTIAction(PlugInto)
;

DefineTAction(Unplug)
;

DefineTIAction(UnplugFrom)
;

DefineTAction(Screw)
;

DefineTIAction(ScrewWith)
    /* limit 'all' for the indirect object to items in inventory */
    getAllIobj(actor, scopeList)
    {
        return scopeList.subset({x: x.isIn(actor)});
    }
;

DefineTAction(Unscrew)
;

DefineTIAction(UnscrewWith)
    /* limit 'all' for the indirect object to items in inventory */
    getAllIobj(actor, scopeList)
    {
        return scopeList.subset({x: x.isIn(actor)});
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Travel Action - this is the base class for verbs that attempt to move
 *   an actor to a new location via one of the directional connections
 *   from the current location.
 *   
 *   Each grammar rule for this action must set the 'dirMatch' property to
 *   a DirectionProd match object that gives the direction.  
 */
DefineIAction(Travel)
    execAction()
    {
        local conn;
        
        /* 
         *   Perform the travel via the connector, if we have one.  If
         *   there's no connector defined for this direction, show a
         *   default "you can't go that way" message. 
         */
        if ((conn = getConnector()) != nil)
        {
            /* 
             *   we have a connector - use the pseudo-action TravelVia with
             *   the connector to carry out the travel 
             */
            replaceAction(TravelVia, conn);
        }
        else
        {
            /* no connector - show a default "can't go that way" error */
            mainReport(&cannotGoThatWayMsg);
        }
    }

    /* get the direction object for the travel */
    getDirection() { return dirMatch != nil ? dirMatch.dir : nil; }

    /*
     *   Get my travel connector.  My connector is given by the travel
     *   link property for this action as defined in the actor's current
     *   location. 
     */
    getConnector()
    {
        /* ask the location for the connector in my direction */
        return gActor.location == nil
            ? nil
            : gActor.location.getTravelConnector(getDirection(), gActor);
    }

    /*
     *   The grammar rules for the individual directions will usually just
     *   create a base TravelAction object, rather than one of the
     *   direction-specific subclasses (NorthAction, etc).  For
     *   convenience in testing the action, though, treat ourself as
     *   matching the subclass with the same direction.  
     */
    actionOfKind(cls)
    {
        /* 
         *   If they're asking about a specific-direction TravelAction
         *   subclass, then we match it if our own direction matches that
         *   of the given subclass, and we fail to match if our direction
         *   doesn't match the given direction.  
         */
        if (cls.ofKind(TravelAction) && cls.getDirection() != nil)
        {
            /* we match if and only if the direction matches */
            return (getDirection() == cls.getDirection());
        }

        /* otherwise, inherit the default handling */
        return inherited(cls);
    }
;

/* for a vague command such as GO, which doesn't say where to go */
DefineIAction(VagueTravel)
    execAction()
    {
        /* simply ask for a direction */
        reportFailure(&whereToGoMsg);
    }
;

/*
 *   This class makes it convenient to synthesize a TravelAction given a
 *   Direction object.  To create a travel action for a direction, use
 *   
 *.     new TravelDirAction(direction)
 *   
 *   where 'direction' is the direction object (northDirection, etc) for
 *   the desired direction of travel.  Note that if you want to use the
 *   resulting object in replaceAction() or one of the similar macros,
 *   you'll need to go directly to the underlying function rather than
 *   using the standard macro, since the macros expect a literal action
 *   name rather than an object.  For example:
 *   
 *.     _replaceAction(gActor, new TravelDirAction(getDirection));
 */
DefineAction(TravelDir, TravelAction)
    construct(dir)
    {
        /* remember my direction */
        dir_ = dir;
    }
    
    /* get my direction */
    getDirection() { return dir_; }
    
    /* my direction, normally specified during construction */
    dir_ = nil
;

/*
 *   To make it more convenient to use directional travel actions as
 *   synthesized commands, define a set of action classes for the specific
 *   directions.  
 */
DefineAction(North, TravelAction)
    getDirection = northDirection
;
DefineAction(South, TravelAction)
    getDirection = southDirection
;
DefineAction(East, TravelAction)
    getDirection = eastDirection
;
DefineAction(West, TravelAction)
    getDirection = westDirection
;
DefineAction(Northeast, TravelAction)
    getDirection = northeastDirection
;
DefineAction(Northwest, TravelAction)
    getDirection = northwestDirection
;
DefineAction(Southeast, TravelAction)
    getDirection = southeastDirection
;
DefineAction(Southwest, TravelAction)
    getDirection = southwestDirection
;
DefineAction(In, TravelAction)
    getDirection = inDirection
;
DefineAction(Out, TravelAction)
    getDirection = outDirection
;
DefineAction(Up, TravelAction)
    getDirection = upDirection
;
DefineAction(Down, TravelAction)
    getDirection = downDirection
;
DefineAction(Fore, TravelAction)
    getDirection = foreDirection
;
DefineAction(Aft, TravelAction)
    getDirection = aftDirection
;
DefineAction(Port, TravelAction)
    getDirection = portDirection
;
DefineAction(Starboard, TravelAction)
    getDirection = starboardDirection
;

/*
 *   Non-directional travel actions 
 */

DefineTAction(GoThrough)
;

DefineTAction(Enter)
;

/*
 *   An internal action for traveling via a connector.  This isn't a real
 *   action, and shouldn't have a grammar defined for it.  The purpose of
 *   this action is to allow real actions that cause travel via a
 *   connector to be implemented by mapping to this internal action, which
 *   we implement on the base travel connector class.  
 */
DefineTAction(TravelVia)
    /* 
     *   The direct object of this synthetic action isn't necessarily an
     *   ordinary simulation object: it could be a TravelConnector instead.
     *   Since callers asking for a direct object almost always expect a
     *   simulation object, returning a non-simulation object here can be
     *   problematic.  To avoid this, we return an empty object list by
     *   default - this ensures that no one who asks for the direct object
     *   of the verb will get back a non-simulation travel connector.  
     */
    getCurrentObjects = []
;

/* "go back" */
DefineIAction(GoBack)
    execAction()
    {
        /* ask the actor to handle it */
        gActor.reverseLastTravel();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Combined pushing-and-traveling action ("push crate north", "drag sled
 *   into cave").  All of these are based on a base action class, which
 *   defines the methods invoked on the object being pushed; the
 *   subclasses provide a definition of the connector that determines
 *   where the travel takes us.  
 */
DefineTAction(PushTravel)
    /*
     *   Carry out the nested travel action for the special combination
     *   push-traveler.  This should carry out the same action we would
     *   have performed for the underlying basic travel.
     *   
     *   This method is invoked by the TravelPushable to carry out a
     *   push-travel action.  The TravelPushable object will first set up
     *   a PushTraveler as the actor's global traveler, and it will then
     *   invoke this method to carry out the actual travel with that
     *   special traveler in effect.  Our job is to provide the mapping to
     *   the correct underlying simple travel action; since we'll be
     *   moving the PushTraveler object, we can move it using the ordinary
     *   non-push travel action as though it were any other traveler.
     *   
     *   This method is abstract - each subclass must define it
     *   appropriately.  
     */
    // performTravel() { }
;

/*
 *   For directional push-and-travel commands, we define a common base
 *   class that does the work to find the connector based on the room's
 *   directional connector.
 *   
 *   Subclasses for grammar rules must define the 'dirMatch' property to
 *   be a DirectionProd object for the associated direction.  
 */
DefineAction(PushTravelDir, PushTravelAction)
    /*
     *   Get the direction we're going.  By default, we return the
     *   direction associated with the dirMatch match object from our
     *   grammar match.  
     */
    getDirection() { return dirMatch.dir; }

    /* carry out the nested travel action for a PushTravel */
    performTravel()
    {
        local conn;
        
        /* ask the actor's location for the connector in our direction */
        conn = gActor.location.getTravelConnector(getDirection(), gActor);

        /* perform a nested TravelVia on the connector */
        nestedAction(TravelVia, conn);
    }
;

/*
 *   To make it easy to synthesize actions for pushing objects, define
 *   individual subclasses for the various directions.
 */
DefineAction(PushNorth, PushTravelDirAction)
    getDirection = northDirection
;

DefineAction(PushSouth, PushTravelDirAction)
    getDirection = southDirection
;

DefineAction(PushEast, PushTravelDirAction)
    getDirection = eastDirection
;

DefineAction(PushWest, PushTravelDirAction)
    getDirection = westDirection
;

DefineAction(PushNorthwest, PushTravelDirAction)
    getDirection = northwestDirection
;

DefineAction(PushNortheast, PushTravelDirAction)
    getDirection = northeastDirection
;

DefineAction(PushSouthwest, PushTravelDirAction)
    getDirection = southwestDirection
;

DefineAction(PushSoutheast, PushTravelDirAction)
    getDirection = southeastDirection
;

DefineAction(PushUp, PushTravelDirAction)
    getDirection = upDirection
;

DefineAction(PushDown, PushTravelDirAction)
    getDirection = downDirection
;

DefineAction(PushIn, PushTravelDirAction)
    getDirection = inDirection
;

DefineAction(PushOut, PushTravelDirAction)
    getDirection = outDirection
;

DefineAction(PushFore, PushTravelDirAction)
    getDirection = foreDirection
;

DefineAction(PushAft, PushTravelDirAction)
    getDirection = aftDirection
;

DefineAction(PushPort, PushTravelDirAction)
    getDirection = portDirection
;

DefineAction(PushStarboard, PushTravelDirAction)
    getDirection = starboardDirection
;

/*
 *   Base class for two-object push-travel commands, such as "push boulder
 *   out of cave" or "drag sled up hill".  For all of these, the connector
 *   is given by the indirect object.
 */
DefineAction(PushTravelViaIobj, TIAction, PushTravelAction)
    /*
     *   Verify the indirect object of the push-travel action.  We'll
     *   remap this to given corresponding simple travel action, and call
     *   that action's verifier.  
     */
    verifyPushTravelIobj(obj, action)
    {
        /* handle this by remapping it to the underlying simple action */
        remapVerify(IndirectObject, gVerifyResults, [action, obj]);
    }
;

DefineTIActionSub(PushTravelThrough, PushTravelViaIobjAction)
    /* 
     *   Carry out the underlying simple travel action.  This simply
     *   performs a GoThrough on my indirect object, as though we had
     *   typed simply GO THROUGH iobj.  The PushTraveler will already be
     *   set up as the actor's special traveler, so the ordinary GO
     *   THROUGH command will move the special PushTraveler object as
     *   though it were the original actor.  
     */
    performTravel() { nestedAction(GoThrough, getIobj()); }
;

DefineTIActionSub(PushTravelEnter, PushTravelViaIobjAction)
    /* carry out the underlying simple travel as an ENTER action */
    performTravel() { nestedAction(Enter, getIobj()); }
;

DefineTIActionSub(PushTravelGetOutOf, PushTravelViaIobjAction)
    /* carry out the underlying simple travel as a GET OUT OF action */
    performTravel() { nestedAction(GetOutOf, getIobj()); }
;

DefineTIActionSub(PushTravelClimbUp, PushTravelViaIobjAction)
    /* carry out the underlying simple travel as a CLIMB UP action */
    performTravel() { nestedAction(ClimbUp, getIobj()); }
;

DefineTIActionSub(PushTravelClimbDown, PushTravelViaIobjAction)
    /* carry out the underlying simple travel as an CLIMB DOWN action */
    performTravel() { nestedAction(ClimbDown, getIobj()); }
;

/*
 *   The "exits" verb.  This verb explicitly shows all of the exits from
 *   the current location. 
 */
DefineIAction(Exits)
    execAction()
    {
        /* 
         *   if we have an exit lister object, invoke it; otherwise,
         *   explain that this command isn't supported in this game 
         */
        if (gExitLister != nil)
            gExitLister.showExitsCommand();
        else
            gLibMessages.commandNotPresent;
    }
;

/* in case the exits module isn't included */
property showExitsCommand, exitsOnOffCommand;

/*
 *   Change the exit display mode.  The grammar must set one of the mode
 *   token properties to a non-nil value, according to which mode the
 *   player selected: on_ for turning on statusline and description lists;
 *   stat_ for turning on only the statusline list; look_ for turning on
 *   only the room description list; and off_ for turning off everything.  
 */
DefineSystemAction(ExitsMode)
    execSystemAction()
    {
        local stat, look;

        /* 
         *   If it's EXITS ON, turn on both statusline and room description
         *   lists.  If it's EXITS LOOK or EXITS STATUS, just turn on one
         *   or the other.  Otherwise, turn both off. 
         */
        stat = (stat_ != nil || on_ != nil);
        look = (look_ != nil || on_ != nil);

        /* update the exit display */
        if (gExitLister != nil)
            gExitLister.exitsOnOffCommand(stat, look);
        else
            gLibMessages.commandNotPresent;
    }
;

/*
 *   Dummy OOPS action for times when OOPS isn't in context.  We'll simply
 *   explain how OOPS works, and that you can't use it right now.  
 */
DefineLiteralAction(Oops)
    execAction()
    {
        /* simply explain how this command works */
        gLibMessages.oopsOutOfContext;
    }

    /* this is a meta-command, so don't consume any time */
    actionTime = 0
;

/* intransitive form of "oops" */
DefineIAction(OopsI)
    doActionMain()
    {
        /* as with OOPS with a literal, simply explain the problem */
        gLibMessages.oopsOutOfContext;
    }

    /* this is a meta-command, so don't consume any time */
    actionTime = 0
;

property disableHints, showHints;

/* hint system - disable hints for this session */
DefineSystemAction(HintsOff)
    execSystemAction()
    {
        if (gHintManager != nil)
            gHintManager.disableHints();
        else
            mainReport(gLibMessages.hintsNotPresent);
    }
;

/* invoke hint system */
DefineSystemAction(Hint)
    execSystemAction()
    {
        if (gHintManager != nil)
            gHintManager.showHints();
        else
            mainReport(gLibMessages.hintsNotPresent);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Parser debugging verbs 
 */

#ifdef PARSER_DEBUG

DefineIAction(ParseDebug)
    execAction()
    {
        local newMode;

        /* 
         *   get the mode - if the mode is explicitly stated in the
         *   command, use the stated new mode, otherwise invert the current
         *   mode 
         */
        newMode = (onOrOff_ == 'on'
                   ? true
                   : onOrOff_ == 'off'
                   ? nil
                   : !libGlobal.parserDebugMode);

        /* set the new mode */
        libGlobal.parserDebugMode = newMode;

        /* mention the change */
        "Parser debugging is now
        <<libGlobal.parserDebugMode ? 'on' : 'off'>>.\n";
    }
;

grammar predicate(ParseDebug):
    'parse-debug' 'on'->onOrOff_
    | 'parse-debug' 'off'->onOrOff_
    | 'parse-debug'
    : ParseDebugAction
;

#endif

