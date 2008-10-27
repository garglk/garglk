/* ------------------------------------------------------------------------ */
/*
 *   "Quit" exception.  This exception does not represent an error
 *   condition, but simply provides a convenient way to terminate the game
 *   at any point in the code.  All of the library exception handlers will
 *   pass this exception up to the main outer loop of the game, which will
 *   simply exit the program with no error when it catches this type of
 *   exception.  
 */
QuitException: Exception
;

/*
 *   "Abort" exception.  This exception can be thrown at most points in
 *   command processing to completely terminate the current turn.  
 */
AbortException: Exception
;

/*
 *   "Exit" exception.  This exception can be thrown at most points in
 *   command processing to skip additional processing on the current
 *   action, and proceed immediately to the next action on the command
 *   line.  
 */
ExitException: Exception
;


/* ------------------------------------------------------------------------ */
/*
*   Quit the game.  This can be called at any point; we simply throw
*   QuitException in order to jump out to and then exit the main game
*   loop.
*/
quit()
{
    throw QuitException;
}

/*
 *   Abort the current turn.  This can be called at most points during
 *   command processing; we'll simply end the current turn, skipping all
 *   remaining processing on the current turn, skipping all fuses and
 *   daemons, and ignoring any following commands on the same command
 *   line.  
 */
abort()
{
    throw AbortException;
}

/*
 *   Exit the current turn.  This can be called at most points during
 *   command processing to skip any further processing for the current
 *   action, and proceed immediately to the next action on the same
 *   command line.
 *   
 *   Note that this routine will not skip additional objects that are part
 *   of the same command.  For example, if this is called while processing
 *   the 'ball' object in the command 'take ball and book,' we will
 *   proceed to process the command for the 'book' object.  
 */
exit()
{
    throw ExitException;
}


/* ------------------------------------------------------------------------ */
/*
*   Command object.  We create an instance of this class for each command
*   we execute.
*/
class Command: object
    /* the actor performing the command */
    actor = nil

    /* the action to be performed - this is an instance of Action */
    action = nil

    /*
     *   The direct object, if any - for an action that takes no direct
     *   object, this will be nil.  Note that this is a single object,
     *   even if the player's command had a list of direct object - we
     *   will create a separate command for each direct object and execute
     *   each command separately.
     */
    dobj = nil

    /*
     *   The indirect object, if any - for an action that takes no
     *   indirect object, this will be nil.
     */
    iobj = nil

    /*
     *   The standard library only defines verbs with zero, one, or two
     *   objects, hence we only define dobj and iobj properties here.
     *   However, there is nothing stopping library extensions or games
     *   from adding actions taking additional objects - actions with
     *   three or more objects should simply add an additional property
     *   here for each additional object.  We stop at two objects only
     *   because that's the most we need for the library actions.
     */
;

/* ------------------------------------------------------------------------ */
/*
 *   Command line tokenizer.  
 */
CommandTokenizer: Tokenizer
    /*
     *   The default tokenizer rules are suitable for command lines, so we
     *   don't need to override anything here.
     */
;

/* ------------------------------------------------------------------------ */
/*
 *   Parse and execute a command line.  'cmd' is a string giving the
 *   command line to be parsed, and 'prod' is the root grammar production
 *   to use in parsing the command.  
 */
processCommandLine(cmd, prod)
{
    /* catch exceptions that cancel the entire command */
    try
    {
        /* catch exceptions that skip to end-of-turn processing */
        try
        {
            local toklist;

            /* perform text-level pre-parsing */
            forEachInstance(PreParseObject, { obj: cmd = obj.preParse(cmd) });

            /* tokenize the command */
            toklist = CommandTokenizer.tokenize(cmd);

            /*
             *   if the command is empty, use the special empty command
             *   processor
             */
            if (toklist[1] == [])
            {
                /* respond to the empty command */
                libMessages.emptyCommandResponse();
            }

            /*
             *   keep going until we've processed all of the tokens on the
             *   command line
             */
            while (toklist[1] != [])
            {
                // $$$ match the token list to 'prod'

                // $$$ resolve noun phrases.  If we have multiple grammar matches,
                // try resolving in the first match first; if that succeeds, we'll
                // consider the first match to be the right match, otherwise we'll
                // try resolving in the second match, and so on until we find a
                // match with resolvable noun phrases or run out of matches.
                // Choose the first successfully resolved match as the winner,
                // and ignore all of the other matches.

                // $$$ remove the matching tokens from toklist, so that
                // toklist contains the remainder of the command line after
                // the current command we just matched

                /* perform token-level pre-parsing */
                forEachInstance(PreParseTokensObject,
                    { obj: obj.preParseTokens(matchToklist) });

                /* execute the command */
                match.execute();
            }
        }
        catch (ExitException exitExc)
        {
            /*
             *   exit - the command line was cancelled, but we're still to
             *   perform end-of-turn processing
             */
        }
    }
    catch (QuitException quitExc)
    {
        /*
         *   a game-terminating exception was thrown - simply re-throw the
         *   exception to our caller to exit from the game
         */
        throw quitExc;
    }
    catch (AbortException abortExc)
    {
        /*
         *   abort - the entire command line was cancelled, including all
         *   end-of-turn processing; this means we're done with the
         *   command line
         */
    }
    catch (Exception exc)
    {
        /* ignore all other exceptions */
        // $$$ but maybe display it, since we consider any other exception
        //     that makes it to this point to be an error, and hence it should
        //     be reported to the user for debugging purposes
    }
}


