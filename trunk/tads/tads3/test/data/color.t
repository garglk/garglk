#include <tads.h>

main(args)
{
    "This is a test of setting text colors.
    <font color=red>Red</font>
    <font color=blue>Blue</font>
    <font color=green>Green</font>
    
    <b>
    <font color=red>Bright Red</font>
    <font color=blue>Bright Blue</font>
    <font color=green>Bright Green</font>
    </b>

    <p>Finally, here's some <b>ordinary text in bold mode</b>, to
    see that we use the parameterized highlight setting rather than
    just high-intensity normal text; and
    <font color=white>explicitly white text <b>in bold</b>, to see
    that we don't use parameters for that;</font> and finally
    some <b>bold text with <font color=white>explicit white</font>
    within.</b>

    <p>
    That's about all for now!";

    for (;;)
    {
        local txt;
        
        "\bEnter a color name, or 'Q' to quit.  Type SCREEN <color>
        to set the screen color, or FULL <fgcolor> <bgcolor> to set both
        the foreground and background colors.\n&gt; ";

        txt = inputLine();
        if (txt == nil || txt == 'q' || txt == 'Q')
            break;

        /* check for special commands */
        if (rexMatch('<nocase>screen +(.+)', txt) != nil)
        {
            "Okay, setting the screen color...\n
            <body bgcolor='<<rexGroup(1)[3]>>'>
            Done! ";
        }
        else if (rexMatch('<nocase>full +(.+) +(.+)', txt) != nil)
        {
            local fg = rexGroup(1)[3];
            local bg = rexGroup(2)[3];
            
            "Trying both foreground and background colors...
            <font color=<<fg>> bgcolor=<<bg>>>
            Here it is: color=<<fg>> and bgcolor=<<bg>>.
            Let's try <b>some bold text</b> too.
            </font>
            Okay, done with that. ";
        }
        else if (txt != '')
        {
            "<font color='<<txt>>'>Here's <<txt>>!  <b>And in
            bold!</b></font>";
        }
    }

    "Bye!\b";
}
