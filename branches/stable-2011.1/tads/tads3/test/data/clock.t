#include <tads.h>

main(args)
{
    local t;
    local stat;
    local sec;
    local lastBeepSec;
    local lastSec;

    /* create the banner window for the clock status line */
    stat = bannerCreate(BannerLast, nil, BannerTypeText,
                        BannerAlignBottom, 0, BannerStyleTabAlign);
    bannerSay(stat,
              '<body bgcolor=yellow text=black><tab align=right>'
              + clockTime(getTime(GetTimeDateAndTime)) + '\n');
    bannerSizeToContents(stat);

    /* show welcome message */
    "Welcome to the clock status program.  This program simply reads input
    lines while keeping a clock updated in the status line.  Type 'Q' when
    you're ready to quit, or CLS to clear the screen.\b>";

    /* read input, updating the clock regularly */
mainLoop:
    for (;;)
    {
        local evt;
        
        /* get input with timeout */
        evt = inputLineTimeout(100);

        /* update the clock if the time has changed */
        t = getTime(GetTimeDateAndTime);
        if (t[8] != lastSec)
        {
            /* update the clock in the banner */
            bannerClear(stat);
            bannerSay(stat,
                      '<body bgcolor=yellow text=black><tab align=right>'
                      + clockTime(t) + '\n');
            bannerFlush(stat);

            /* remember the last time we updated */
            lastSec = t[8];
        }

        /* check the event */
        switch(evt[1])
        {
        case InEvtTimeout:
            /* timeout - if we're at a 15-second mark, say so */
            sec = getTime(GetTimeDateAndTime)[8];
            if (sec != lastBeepSec && sec % 15 == 0)
            {
                /* cancel the input line in progress */
                inputLineCancel(nil);

                /* mention the 15-second timer */
                "\b - beep -\b";

                /* set up another prompt and keep going */
                ">";

                /* 
                 *   note the last time we beeped, so we don't beep again
                 *   during the same second 
                 */
                lastBeepSec = sec;
            }
            break;

        case InEvtLine:
            /* input line - show it */
            "You typed: [<<evt[2]>>]\b";

            /* if they typed 'q', we're done */
            switch (evt[2].toLower())
            {
            case 'q':
                "Done.\b";
                break mainLoop;

            case 'cls':
                clearScreen();
                break;
            }

            /* show another prompt and keep going */
            ">";
            break;

        case InEvtEof:
            /* 
             *   end of file on input - the user must have forced the user
             *   interface window closed 
             */
            break mainLoop;
        }
    }
}

/*
 *   convert a system time value to an "hh:mm:ss am/pm" string 
 */
clockTime(t)
{
    local str;
    local ampm;

    /* format the time string */
    ampm = t[6] >= 12 ? ' pm' : ' am';
    str = t[6] == 0 ? '12' : t[6] > 12 ? toString(t[6] - 12) : toString(t[6]);
    str += ':';
    str += t[7] < 10 ? '0' + t[7] : t[7];
    str += ':' + (t[8] < 10 ? '0' + t[8] : t[8]);
    str += ampm;

    /* return the time */
    return str;
}
