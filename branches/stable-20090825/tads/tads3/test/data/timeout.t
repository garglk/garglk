/*
 *   input-with-timeout test
 */

#include <tads.h>

main(args)
{
    local cnt;

    /* show our introductory text */    
    "Welcome to the timeout tester!  Type QUIT to end the program.\b";

    /* no timeouts so far */
    cnt = 0;

    /* keep going until they enter 'quit' */
    for (;;)
    {
        local evt;
        local endTime;

        /* calculate the maximum time to go without adding any comments */
        endTime = getTime(GetTimeTicks) + 10000;

        /* show our prompt */
        "-> ";

        /* 
         *   get input until we reach the user enters the line of text, or
         *   we reach the ending time 
         */
    readLoop:
        while (getTime(GetTimeTicks) < endTime)
        {
            /* get input, with a one-second timeout */
            evt = inputLineTimeout(500);

            /* check what we have */
            switch (evt[1])
            {
            case InEvtEof:
                /* error reading input - abort */
                "\bError reading input!\b";
                return;

            case InEvtTimeout:
                /* count the timeout, but just keep looping */
                ++cnt;
                break;

            case InEvtLine:
                /* we got a line of input - we're done */
                break readLoop;
            }
        }

        /* check why we stopped looping */
        if (evt[1] == InEvtTimeout)
        {
            /*
             *   We timed out.  Show a comment about it, and then go back
             *   for more. 
             */
            inputLineCancel(nil);
            "\bSo far it looks like you typed \"<<evt[2]>>\".
            No hurry; just keep typing as long as you'd like.\b";
        }
        else
        {
            /* if they typed 'quit', we're done */
            if (evt[2].toLower() == 'quit')
            {
                "Done!\b";
                break;
            }

            /*
             *   We read the input.  Mention how many times we timed out
             *   while they were typing. 
             */
            "Thanks for the input.  While you were typing, we processed
            a total of <<cnt>> timeout interruptions.  Your input
            text was \"<<evt[2]>>\".\b";

            /* reset the counter */
            cnt = 0;
        }
    }
}

