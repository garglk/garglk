#include <adv.t>
#include <std.t>

startroom: room
    sdesc = "Start Room"
    ldesc = "This is a very boring room with no exits. "
;

myfunc: external function;

xtestVerb: deepverb
    verb = 'xtest'
    sdesc = "xtest"
    action(actor) =
    {
        say(myfunc(123));  "\n";
	say(myfunc('hello there'));
    }
;

