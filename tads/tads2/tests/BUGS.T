/* Copyright (c) 1993 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  bugs.t - regression for various bugs
Function
  tests a number of operations that have been buggy in the past,
  plus some assorted new features
Notes
  none
Modified
  01/15/93 MJRoberts     - creation
*/

#include <adv.t>
#include <std.t>

startroom: room
    sdesc = "Start Room"
    ldesc = "There's nowhere to go."
;

bones: item
    location = startroom
    noun = 'bone' 'bones' 'skeleton'
    sdesc = "skeleton"
    ldesc = "It's the skeleton of a previous explorer. "
    iobjGen(a, v, d, p) = { self.dobjGen(a, v, d, p); }
    doOpen(actor) = { "You can't open a skeleton!!!!!"; }
    dobjGen(a, v, i, p) =
    {
	if (v <> inspectVerb)
	{
	    "A bolt of lightning leaps across the room as you
	    disturb the skeleton. \"Fool!  You shall be punished
	    for this desecration!\" a voice bellows. ";
	    exit;
	}
    }
;
    
rustyKnife: item
    location = startroom
    noun = 'knife'
    adjective = 'rusty'
    sdesc = "rusty knife"
;

fuse1: function(parm)
{
/*    tmp := parm; */
    "\bThis is fuse1\b";
}

testobj: object testprop = { "\bThis is testobj.testprop\b"; };

showlist: function(l)
{
    local i, tot;
    
    "[ ";
    for (tot := length(l), i := 1 ; i <= tot ; ++i)
	"<< l[i] >> ";
    "]";
}

andlist: function(l1, l2)
{
    showlist(l1); " and "; showlist(l2); " = ";
    showlist(intersect(l1, l2)); "\n";
}

isectVerb: deepverb verb = 'isect'
    action(actor) =
    {
	andlist([1 2 3 4 5 6], [3 5 7]);
	andlist([3 5 7], [1 2 3 4 5 6]);
	andlist([1 1 1], [2 2 2]);
	andlist([1 1 1], [1 1]);
	andlist([1 1 1], [1 2]);
	andlist(['abc' 'def' 'ghi'], ['xyz' 'abc' 'zzz']);
    }
;

testVerb: deepverb verb = 'test' action(actor) = { foo3(); } ;
test2Verb: deepverb verb = 'test2'
    action(actor) =
    {
	"Setting fuse fuse1 in 3 turns\n";
	setfuse(fuse1, 3, nil);
	
	"Setting notify testobj, &testprop, 3\n";
	notify(testobj, &testprop, 3);
    }
;

test3verb: deepverb verb = 'runfuses'
    action(actor) =
    {
	rundaemons();
	if (runfuses()) "Fuses ran.\n"; else "No fuses ran.\n";
    }
;

foo4verb: deepverb verb = 'foo4' action(actor) = { foo4(); } ;
foo4: function
{
    local t := gettime();
    local i;

    "The time is: "; showlist(t); "\n";
    
    for (i := 1 ; i <= 9 ; ++i) "<< t[i] >> "; "\n";

    "getfuse(fuse1, nil) = "; say(getfuse(fuse1, nil));
    "\ngetfuse(testobj, &testprop) = "; say(getfuse(testobj, &testprop));
    "\n";
}

foo3a: function(a, b) { "foo3a: << a >>...<< b >>...\n"; }

foo3: function
{
    local l := [1 2 3 4 5];
    local x;
    
    foo3a(1 = 1 ? '1 = 1' : '1 <> 1!!!', 2);
    
    l[3] := 9;
    showlist(l); " - should be [1 2 9 4 5]\n";
    l[5] := 10;
    showlist(l); " - should be [1 2 9 4 10]\n";
    
    l[4] += 3;
    showlist(l); " - should be [1 2 9 7 10]\n";
    x := --l[2];
    showlist(l); " - should be [1 1 9 7 10]\n";
    
    global.tp := [1 2 3];
    global.tp[2] += 3;
    showlist(global.tp); " - should be [1 5 3]\n";
}

foo2verb: deepverb verb = 'foo2' action(actor) = { foo2(); } ;
foo2: function
{
    local i;
    "Enter number> ";  i := cvtnum(input());
    switch(i)
    {
    case 1:
	"One";
	break;
    case 2:
	"Two";
	break;
    case 3:
	"Three";
	break;
    default:
	"Not 1, 2, or 3";
    }
    "\n";
    
    switch(i)
    {
    case 4:
	"Four - enter another> ";
	switch(cvtnum(input()))
	{
	case 1:
	    "The second number is one";
	    break;
	case 2:
	    "The second number is two";
	    break;
	default:
	    "The second number is not 1 or 2";
	}
	break;
    default:
	"Not 4";
    }
    "\n";
}

fooverb: deepverb verb = 'foo' action(actor) = { foo(); } ;
foo: function
{
    local i;
    
    "Before goto...\n";
    goto testlabel;
    "After goto (should never be here!)...\n";
testlabel:
    "After testlabel...\n";
    
    for (i := 0 ; i < 10 ; ++i)
    {
	if (i = 5) continue;
	"i = << i >>\n";
    }
    
    i := [1 2 3];
    "datatype(i) = <<datatype(i)>>\n";
    i := rand(5);
    "datatype(i) = <<datatype(i)>>\n";
}
