/*
 *   Test of t3core.  Compile with '-nodef', since we don't want to pick up
 *   the normal library startup code.  
 */

#include <t3.h>
#include <tadsgen.h>
#include <systype.h>
#include <core.h>

/* 
 *   this is the low-level main entrypoint - the VM calls this directly at
 *   program start-up 
 */
_main(args)
{
    /* set our default display function */
    t3SetSay(say);

    /* invoke our normal 'main' */
    main(args);
}

main(args)
{
    "Hello, world!\n";

    "\nThis is a little test of the t3core program.  We'll just\n
    read text from the keyboard and echo it to the display.\n
    When you're done, enter a blank line to quit...\n\n";
    for (;;)
    {
        local str;
        
        "Enter some text> ";
        str = readText();
        if (str == '')
            break;

        "You entered \"<<str>>\"!\n\n";
    }

    "Thanks for joining us!\n";
}

say(txt)
{
    /* ignore nil values */
    if (txt == nil)
        return;
    
    /* write text to the t3core sample output routine (from core.h) */
    displayText(txt);
}
