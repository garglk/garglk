/*
 *   undo.t - test of savepoint/undo
 */

#include "tads.h"
#include "t3.h"
#include "t3test.h"


_say_embed(str) { tadsSay(str); }

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime Error: <<errno_>>"
    errno_ = 0
;

_main(args)
{
    try
    {
        t3SetSay(_say_embed);
        main();
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

obj1: object
    construct(id) { id_ = id; }
    sdesc = "This is obj1(<<id_>>, <<t3test_get_obj_id(self)>>)"
    id_ = 'original'
    p1 = 1
    p2 = 2
    p3 = 3
    p4 = nil
    finalize() { "Finalizer: obj1(<<id_>>, <<t3test_get_obj_id(self)>>)\n"; }
;

main()
{
    for (local i = 0 ; i < 3 ; ++i)
    {
        "*** Pass <<i>> ***\b";

        "Step One\n";
        savepoint();
        t3RunGC();

        obj1.p1 = 'one';
        obj1.p2 = 'two';
        obj1.p3 = 'three';
        obj1.p4 = new obj1('step one');
        "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3>>,
        p4 = << obj1.p4 == nil ? "nil" : obj1.p4.sdesc >>\n";

        "\bStep Two\n";
        savepoint();
        t3RunGC();

        obj1.p1 += '111';
        obj1.p2 += '222';
        obj1.p3 += '333';
        obj1.p4 = new obj1('step two');
        "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3>>,
        p4 = << obj1.p4 == nil ? "nil" : obj1.p4.sdesc >>\n";

        "\bStep Three\n";
        savepoint();
        t3RunGC();

        obj1.p1 = 'x';
        obj1.p2 = 'y';
        obj1.p3 = 'z';
        obj1.p4 = new obj1('step three');
        "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3>>,
        p4 = << obj1.p4 == nil ? "nil" : obj1.p4.sdesc >>\n";

        "\bUndoing: <<bool2str(undo())>>\n";
        t3RunGC();
        "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3>>,
        p4 = << obj1.p4 == nil ? "nil" : obj1.p4.sdesc >>\n";

        "\bStep Three Prime\n";
        savepoint();
        t3RunGC();

        obj1.p1 = 'a';
        obj1.p2 = 'b';
        obj1.p3 = 'c';
        obj1.p4 = new obj1('step three prime');
        "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3>>,
        p4 = << obj1.p4 == nil ? "nil" : obj1.p4.sdesc >>\n";

        "\bUndoing: <<bool2str(undo())>>\n";
        t3RunGC();
        "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3>>,
        p4 = << obj1.p4 == nil ? "nil" : obj1.p4.sdesc >>\n";

        "\bUndoing: <<bool2str(undo())>>\n";
        t3RunGC();
        "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3>>,
        p4 = << obj1.p4 == nil ? "nil" : obj1.p4.sdesc >>\n";

        "\bUndoing: <<bool2str(undo())>>\n";
        t3RunGC();
        "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3>>,
        p4 = << obj1.p4 == nil ? "nil" : obj1.p4.sdesc >>\n";

        "\bUndoing: <<bool2str(undo())>>\n";
        t3RunGC();
        "obj1.p1 = <<obj1.p1>>, p2 = <<obj1.p2>>, p3 = <<obj1.p3>>,
        p4 = << obj1.p4 == nil ? "nil" : obj1.p4.sdesc >>\n";

        "\b";
    }
}

bool2str(val)
{
    return val ? 'true' : 'nil';
}
