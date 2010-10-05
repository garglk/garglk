#include "tads.h"
#include "t3.h"

_main(args)
{
    t3SetSay(&_say_embed);
    a();
}

_say_embed(str) { tadsSay(str); }

class Exception: object
    construct(msg) { exceptionMessage = msg; }
    exceptionMessage = ''
;

class ResourceError: Exception;
class ParsingError: Exception;


a()
{
    b(1);
    b(2);
    b(3);
}

b(x)
{
    "This is b(<<x>>)\n";
    
    try
    {
        c(x);
    }
    catch (Exception exc)
    {
        "b: Caught an exception: <<exc.exceptionMessage>>\n";
    }
    
    "Done with b(<<x>>)\n";
}

c(x)
{
    "This is c(<<x>>)\n";
    
    try
    {
        d(x);
    }
    catch(ParsingError perr)
    {
        "c: Caught a parsing error: <<perr.exceptionMessage>>\n";
    }
    finally
    {
        "In c's finally clause\n";
    }
    
    "Done with c(<<x>>)\n";
}

d(x)
{
    "This is d(<<x>>)\n";
    e(x);
    "Done with d(<<x>>)\n";
}

e(x)
{
    "This is e(<<x>>)\n";

    if (x == 1)
    {
        "Throwing resource error...\n";
        throw new ResourceError('some resource error');
    }
    else if (x == 2)
    {
        "Throwing parsing error...\n";
        throw new ParsingError('some parsing error');
    }
    
    "Done with e(<<x>>)\n";
}
