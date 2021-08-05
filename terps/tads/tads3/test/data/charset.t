/*
 *   character set mappings - this file was prepared with code page 1252
 */

#include "t3.h"
#include "t3test.h"
#include "tads.h"

_say_embed(str) { tadsSay(str); }

_main(args)
{
    t3SetSay(&_say_embed);
    main();
}

main()
{
    local str;
    
    "Welcome to the character set test!\n
    Here are some accented A's: ÀÁÂÃÄÅàáâãäå\n
    How about some accented E's: ÈÉÊËèéêë\n
    That should do it!\n";

    /* set the string to something wacky to look at in the debugger */
    str = 'ÀÁÂÃÄÅàáâãäå\u1234\uABCD\u7f7f/Hello!\n\b\t\015\001\\$@';	

    "Try entering some text!\n> ";
    str = inputLine();
    "You entered \"<<str>>\" = ";
    for (local i = 1 ; i <= str.length() ; ++i)
        "<<toString(t3test_get_charcode(str.substr(i, 1)), 16)>> ";
    "\n";
}

