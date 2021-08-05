/*
 *   This tests using 'replaced()' (i.e., a call to the original version of
 *   a function that was replaced with 'modify') from within an anonymous
 *   function within the modifier function. 
 */

main(args)
{
    myfunc();
}

myfunc()
{
    "This is the original myfunc().\n";
}

modify myfunc()
{
    "This is the replaced myfunc().\n";
    "Calling replaced()...[\n";
    replaced();
    "]... back from replaced()\n";

    local f = new function()
    {
        "In anon - calling replaced...[\n";
        replaced();
        "]...back in anon\n";
    };

    "Again, using an anonymous function...[\n";
    f();
    "]...back from anon\n";
}

