/*
 *   HTML status line test
 */

#include "t3.h"
#include "tads.h"

_say_embed(str) { tadsSay(str); }

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime Error: <<errno_>>"
    errno_ = 0
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
    "<center>
    <i><b><font size=+2>
    Welcome!
    </font></b></i>
    </center>

    <br><br><br>
    <font color=blue face='TADS-Sans' size=+1>
    Type QUIT to quit, SCORE to add points to the score.\b
    </font>";
    
    for (;;)
    {
        local cmd;

        /* update the status line */
        showStatus();

        /* count the turn */
        ++global.turncount;

        /* display the prompt and wait for a command */
        "\b&gt;<font face=TADS-Input> ";
        cmd = inputLine();
        "</font>";

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
    local scoreStr = spellNum(global.score);
    local turnStr = spellNum(global.turncount);

    "<banner id=StatusLine height=previous border>
    <body bgcolor=statusbg text=statustext>
    <b>";

    tadsSay(global.statmsg);

    "</b><tab align=right>
    <i><<scoreStr>>/<<turnStr>>
    </i></banner>";
}

spellNum(num)
{
    local str;
    local neg = nil;

    /* start with an empty string */
    str = '';

    /* if it's zero, just return "zero" */
    if (num == 0)
        return 'zero';

    /* if it's less than zero, note it but work with the absolute value */
    if (num < 0)
    {
        neg = true;
        num = -num;
    }

    /* add the billions if we're in the billions */
    if (num >= 1000000000)
    {
        str += spellNum(num / 1000000000) + ' billion ';
        num %= 1000000000;
    }

    /* add the millions */
    if (num >= 1000000)
    {
        str += spellNum(num / 1000000) + ' million ';
        num %= 1000000;
    }

    /* add the thousands */
    if (num >= 1000)
    {
        str += spellNum(num / 1000) + ' thousand ';
        num %= 1000;
    }

    /* 
     *   add the hundreds (just a single-digit number of hundreds can
     *   remain, because we've already dispatched the thousands) 
     */
    if (num >= 100)
    {
        str += spellNum(num / 100) + ' hundred ';
        num %= 100;
    }

    /* 
     *   if the remainder is zero, we've unnecessarily left a trailing
     *   space in the number - if this is the case, remove the trailing
     *   space 
     */
    if (num == 0)
        str = str.substr(1, str.length() - 1);

    /* 
     *   Add the tens (just a single-digit number of tens can remain,
     *   because we've already dispatched the hundreds).  If we're in the
     *   teens, add nothing now; we consider the teens to be a number of
     *   units because of the irregular naming English uses for the teens. 
     */
    if (num >= 20)
    {
        local tens = ['twenty', 'thirty', 'forty', 'fifty', 'sixty',
                      'seventy', 'eighty', 'ninety'];

        /* add the tens name */
        str += tens[(num - 10)/10];
        num %= 10;

        /* add a dash if we have any units left */
        if (num != 0)
            str += '-';
    }

    /* add the units (or teens) */
    if (num != 0)
    {
        local units = ['one', 'two', 'three', 'four', 'five', 'six',
                       'seven', 'eight', 'nine', 'ten', 'eleven',
                       'twelve', 'thirteen', 'fourteen', 'fifteen',
                       'sixteen', 'seventeen', 'eighteen', 'nineteen'];

        /* add the units name */
        str += units[num];
    }

    /* if it's negative, put it in parentheses */
    if (neg)
        str = '(' + str + ')';

    /* return the result */
    return str;
}

