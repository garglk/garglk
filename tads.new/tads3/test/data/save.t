/*
 *   test of save/restore 
 */

#include "tads.h"
#include "t3.h"

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime Error: <<errno_>>"
    errno_ = 0
;

_say_embed(str) { tadsSay(str); }

_main(args)
{
    try
    {
        t3SetSay(_say_embed);
        main(args);
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

obj1: object
    construct(nm) { sdesc = nm; }
    p1 = 'original obj1.p1'
    p2 = 'original obj1.p2'
    p3 = nil
    p4 = 'original obj1.p4'
    sdesc = 'obj1'
;

obj2: object
    construct(nm) { sdesc = nm; }
    p1 = 'original obj2.p1'
    p2 = 'original obj2.p2'
    p3 = nil
    p4 = 'original obj2.p4'
    sdesc = 'obj1'
;

globals: object
    obj3_ = nil
;

main(args)
{
    local obj3;

    obj3 = new obj1('obj3 pre-save');
    obj3.p1 = obj3.p2 = 'first new obj3';
    obj3.p1 += '.p1';
    obj3.p2 += '.p2';

    obj1.p1 += ' Modified!';
    obj2.p2 += ' Modified!';

    obj1.p3 = obj2.p3 = obj3;


    "Pre-save:\n";
    "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3.sdesc>>\n";
    "obj2.p1 = <<obj2.p1>>, p2 = <<obj2.p2>>, p3 = <<obj2.p3.sdesc>>\n";
    "obj3.p1 = <<obj3.p1>>, p2 = <<obj3.p2>>, p4 = <<obj3.p4>>\n";
    "\b";

    "Saving...\n";
    globals.obj3_ = obj3;
    saveGame('test.t3v');
    "Saved!\n";

    obj3 = new obj2('obj3 post-save');
    obj3.p1 = obj3.p2 = 'second new obj3';
    obj3.p1 += '.p1';
    obj3.p2 += '.p2';
    
    obj1.p1 += ' Mod Post Save';
    obj1.p2 += ' Mod Post Save';
    obj2.p1 += ' Mod Post Save';
    obj2.p2 += ' Mod Post Save';
    
    obj1.p3 = obj2.p3 = obj3;
    
    "Pre-Restore:\n";
    "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3.sdesc>>\n";
    "obj2.p1 = <<obj2.p1>>, p2 = <<obj2.p2>>, p3 = <<obj2.p3.sdesc>>\n";
    "obj3.p1 = <<obj3.p1>>, p2 = <<obj3.p2>>, p4 = <<obj3.p4>>\n";
    "\b";
    
    "Restoring...\n";
    restoreGame('test.t3v');
    obj3 = globals.obj3_;
    "Restored!\n";

    "Post-Restore:\n";
    "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3.sdesc>>\n";
    "obj2.p1 = <<obj2.p1>>, p2 = <<obj2.p2>>, p3 = <<obj2.p3.sdesc>>\n";
    "obj3.p1 = <<obj3.p1>>, p2 = <<obj3.p2>>, p4 = <<obj3.p4>>\n";
    "\b";

    "Restarting...\n";
    restartGame();
    _reinit(789, args);
}

_reinit(ctx, args)
{
    try
    {
        t3SetSay(_say_embed);
        reinit(ctx);
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

reinit(ctx)
{    
    "This is reinit!  ctx = <<ctx>>\n";

    obj1.p3 = obj2.p3 = new obj1('obj3 in reinit');
    "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3.sdesc>>\n";
    "obj2.p1 = <<obj2.p1>>, p2 = <<obj2.p2>>, p3 = <<obj2.p3.sdesc>>\n";
    "\b";

    "Calling test1...\n";
    test1();
    "Back in reinit\n";

    "Restoring...\n";
    restoreGame('test.t3v');
    "Restored!\n";

    "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3.sdesc>>\n";
    "obj2.p1 = <<obj2.p1>>, p2 = <<obj2.p2>>, p3 = <<obj2.p3.sdesc>>\n";
    "\b";
}

test1()
{
    local x;

    x = 'hello';
    x += ' ';
    x += 'there!';

    "In test1 - x = '<<x>>'; calling test2...\n";
    test2();
    "Back in test1 - x = '<<x>>'\n";
}

test2()
{
    "In test2\n";

    obj1.p2 += ' test2-modified';
    obj2.p1 += ' test2-modified';
    "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3.sdesc>>\n";
    "obj2.p1 = <<obj2.p1>>, p2 = <<obj2.p2>>, p3 = <<obj2.p3.sdesc>>\n";
    "\b";

    "Saving...\n";
    saveGame('test.t3v');
    "Saved.\n";

    obj1.p1 += ' post-save-test2-modified';
    obj2.p2 += ' post-save-test2-modified';
    
    "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3.sdesc>>\n";
    "obj2.p1 = <<obj2.p1>>, p2 = <<obj2.p2>>, p3 = <<obj2.p3.sdesc>>\n";
    "\b";
}

