/*
 *   anonymous functions
 */

#include "tads.h"
#include "t3.h"

preinit()
{
}

main(args)
{
    local f1;
    local f2;
    local i = 1;
    local cnt = 0;

    "Long Form:\b";

    "Before iterate: cnt = <<cnt>>\n";
    iterate(10, new function { "."; ++cnt; });
    "After iterate(10): cnt = <<cnt>>\n";

    f1 = new function(x) { "this is anon1(<<x>>)!!!\n"; };
    f2 = new function(x, y) { "this is anon2(<<x>>, <<y>>)!!!\n"; };

    f1(1);
    f1(2);
    f2(1, 2);
    f2(3, 4);

    callback(f1, 111);
    callback(f2, 222, 333);
    callback(new function(x, y, z)
             { "this is anon3(<<x>>, <<y>>, <<z>>)!!!!!\n"; },
             'one', 'two', 'three');

    callback(new function { for (local i = 1 ; i < 10 ; ++i) "+"; "\n"; } );
    "after callback: i = <<i>>\n";

    callback(new function { iterate(5, new function { "@"; ++cnt; }); });
    "\ncnt = <<cnt>>\n";

    Sub.m1(888);
    Sub.m1(999);

    "\bShort Form:\b";

    "iterating...\n";
    iterate(5, {: "- count = <<++cnt>>\n"});
    "...done\n";
}

iterate(cnt, f, [args])
{
    for (local i = 0 ; i < cnt ; ++i)
        f(args...);
}

callback(f, [args])
{
    "Callback: ";
    f(args...);
}

class Base: object
    sdesc = "Base"
    m1(x)
    {
        "This is Base.m1(<<x>>) - self = <<self.sdesc>>";
    }
;

Sub: Base
    sdesc = "Sub"
    m1(x)
    {
        "This is Sub.m1(<<x>>) - self = <<self.sdesc>>\n";
        iterate(5, new function { "Sub.m1 Iter: self=<<self.sdesc>>\n"; });
        callback(new function{ "Sub.m1 ["; inherited.m1(x+1); "] done\n"; });
        callback(new function{ "Prop test: subProp = <<subProp>>\n"; });
    }
    subProp = "This is Sub.p!"
;

