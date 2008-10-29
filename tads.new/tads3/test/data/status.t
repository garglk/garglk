/*
 *   status line test
 */

#include "t3.h"
#include "tads.h"

_say_embed(str) { tadsSay(str); }

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime Error: <<exceptionMessage>>"
    errno_ = 0
    exceptionMessage = ''
;

global: object
    turncount = 0
    score = 0
    statmsg = 'Start Room'
;

_main(args)
{
    try
    {
        t3SetSay(&_say_embed);
        main();
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

main()
{
    "Welcome! Type QUIT to quit, SCORE to add points to the score, @file
    to read a script file.\n";
    
    for (;;)
    {
        local cmd;

        /* update the status line */
        showStatus();

        /* count the turn */
        ++global.turncount;

        /* display the prompt and wait for a command */
        "> ";
        cmd = inputLine();

        /* check for a script file specification */
        if (rexMatch(' *@(@?)(!?) *(.*)', cmd) != nil)
        {
            local g;
            local flags = 0;
            
            /* if an extra @ was specified, turn off MORE mode */
            g = rexGroup(1);
            if (g != nil && g[2] != 0)
                flags |= ScriptFileNonstop;

            /* if ! was specified, turn on QUIET mode */
            g = rexGroup(2);
            if (g != nil && g[2] != 0)
                flags |= ScriptFileQuiet;

            /* get the filename part and open the script file */
            g = rexGroup(3);
            setScriptFile(g[3], flags);

            /* done with this command */
            continue;
        }

        /* see what we have */
        switch(cmd.toLower())
        {
        case 'quit':
            "Bye!\n";
            return;

        case 'score':
            "Adding ten points to the score.\n";
            global.score += 10;
            break;

        default:
            "Ignored.\n";
            break;
        }
    }
}

showStatus()
{
    /* 
     *   Update the score part.  Note that we must update the entire
     *   status line in order for the score portion to show up on the
     *   screen, so set the right half before we do the update. 
     */
    statusRight(cvtStr(global.score) + '/' + cvtStr(global.turncount));

    /* display the status line */
    statusMode(StatModeStatus);
    tadsSay(global.statmsg + '\n');
    statusMode(StatModeNormal);
}

