#include <tads.h>
#include <t3.h>
#include <vector.h>

main(args)
{
    "initial: obj1.v = <<sayVec(obj1.v)>>\n";

    obj1.v += 'd';
    obj1.v += 'e';
    obj1.v += 'f';
    obj1.v += 'g';
    obj1.v += 'h';

    "after changes: obj1.v = <<sayVec(obj1.v)>>\n";

    "restarting...\n";
    restartGame();
    restarter(1, args);
}

restarter(ctx, args)
{
    /* perform post-load initialization again, now that we're reloaded */
    initAfterLoad();

    "restarted!\n";

    "obj1.v = <<sayVec(obj1.v)>>\n";

    obj1.v += 'x';
    "more changes: <<sayVec(obj1.v)>>\n";
}

sayVec(v)
{
    local first;
    
    "[";
    first = true;
    foreach (local x in v)
    {
        if (!first)
        {
            ", ";
            first = nil;
        }
        "<<x>>";
    }
    "]";
}

obj1: object
    v = nil
;

myPreiniter: PreinitObject
    execute()
    {
        obj1.v = new Vector(5);
        obj1.v += 'a';
        obj1.v += 'b';
        obj1.v += 'c';
    }
;

