#include <tads.h>

myObj: object
    p1 = 'hello'

    propertyset 'test1*' (x,,y)
    {
        p2() { return 'foo'; }
    }
    propertyset 'test2*' (x*y)
    {
        p3() { return 'bar'; }
    }
    propertyset 'test3*' (x,)
    {
        p4() { return 'blah'; }
    }
    propertyset 'test4*' (,x)
    {
        p5() { return 'blech'; }
    }
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
