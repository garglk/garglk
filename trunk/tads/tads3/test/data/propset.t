#include <tads.h>

myObj: object
    p1 = 'hello'

    propertyset '*DobjTake'
    {
        p2 = 'foo'
        p3 = 'bar'
    }

    propertyset 'verDobj*' (actor, *)
    {
        Take { "verDobjTake - actor=<<actor>>\n"; }
        Drop() { "verDobjDrop - actor=<<actor>>\n"; }
        PutIn(iobj) { "verDobjPutIn - actor=<<actor>>, iobj=<<iobj>>\n"; }
        PutOn(x, y) { "verDobjPutOn - actor=<<actor>>, x=<<x>>, y=<<y>>\n"; }
    }

    propertyset 'verIobj*' (a, *, b)
    {
        Take { "verIobjTake - a=<<a>>, b=<<b>>\n"; }
        Drop() { "verIobjDrop - a=<<a>>, b=<<b>>\n"; }
        PutIn(iobj) { "verIobjPutIn - a=<<a>>, iobj=<<iobj>>, b=<<b>>\n"; }
        PutOn(x, y) { "verIobjPutOn - a=<<a>>, x=<<x>>, y=<<y>>, b=<<b>>\n"; }
    }

    propertyset 'dobj*' (*, actor)
    {
        Take { "dobjTake - actor=<<actor>>\n"; }
        Drop() { "dobjDrop - actor=<<actor>>\n"; }
        PutIn(iobj) { "dobjPutIn - iobj=<<iobj>>, actor=<<actor>>\n"; }
        PutOn(dobj, iobj) { "dobjPutOn - dobj=<<dobj>>, iobj=<<iobj>>,
                             actor=<<actor>>\n"; }
    }

    p4 = 'goodbye'
;

main(args)
{
    "myObj.p1 = <<myObj.p1>>\n";
    "myObj.p2DobjTake = <<myObj.p2DobjTake>>\n";
    "myObj.p3DobjTake = <<myObj.p3DobjTake>>\n";
    "myObj.p4 = <<myObj.p4>>\n";

    myObj.verDobjTake(123);
    myObj.verDobjDrop(987);
    myObj.verDobjPutIn(45, 67);
    myObj.verDobjPutOn(78, 89, 91);

    myObj.verIobjTake(1, 2);
    myObj.verIobjDrop(3, 4);
    myObj.verIobjPutIn(5, 6, 7);
    myObj.verIobjPutOn(8, 9, 10, 11);

    myObj.dobjTake(111);
    myObj.dobjDrop(222);
    myObj.dobjPutIn(333, 444);
    myObj.dobjPutOn(555, 666, 777);
}
