#include <tads.h>
#include <dynfunc.h>

obj1: object
    name = 'obj1'

    ctxTests()
    {
        local i = 1;
        
        obj3.setMethod(
            &a, {x: "shared ctx: x=<<x>>, self=<<self.name>>,
                    defobj=<<definingobj.name>>\n" });
        obj3.setMethod(
            &b, new method(x) { "unshared ctx: x=<<x>>, self=<<self.name>>,
                 defobj=<<definingobj.name>>\n"; });

        obj3.setMethod(
            &c, new method(x) { "unshared ctx 2: x=<<x>>, i=<<i++>>,
                self=<<self.name>>, defobj=<<definingobj.name>>\n"; });

        obj3.a(11);
        obj3.b(12);
        obj3.c(13);
        obj4.a(14);
        obj4.b(15);
        obj4.c(16);
        obj5.a(17);
        obj5.b(18);
        obj5.c(19);
    }
;

obj2: object
    name = 'obj2'
    d(n) { "This is <<name>>.d - n=<<n>>\n"; }
    e = "This is e"
    location = obj1
;

obj3: object
    name = 'obj3'
    location = obj2
;

obj4: obj3
    name = 'obj4'
;

obj5: obj4
    name = 'obj5'
;


property a, b, c, d, e;

method m_topLoc()
{
    return location == nil ? self : location;
}
method m_getLoc() { return location; }

fopt(a, b, c = 0)
{
    return a + b + c;
}

testConcat([args])
{
    local ret = '';
    for (local s in args)
        ret += s;
    return ret;
}

main(args)
{
    local i = 100;

    obj1.setMethod(&a, { x : "This is a - x=<<x>>, i=<<i>>\n" });
    obj1.setMethod(&b, { y : "This is b - y=<<y>>\n" });
    obj1.setMethod(&c, 'This is c\n');
    obj1.setMethod(&d, obj2.getMethod(&d));
    obj1.setMethod(&e, testConcat('This is e: ', makeString('Test... ', 5),
                                  '\n'));

    obj1.a(200);
    obj1.b(300);
    obj1.c;
    obj1.d(400);
    obj1.e;
    "\b";

    local s = obj2.getMethod(&e);
    "obj2.s = [<<s>>], dataType(s) = <<dataType(s)>>\n";

    obj3.setMethod(&a, obj1.getMethod(&a));
    obj3.setMethod(&b, obj1.getMethod(&b));
    obj3.setMethod(&c, obj1.getMethod(&c));
    obj3.setMethod(&d, obj1.getMethod(&d));

    obj3.a(2000);
    obj3.b(3000);
    obj3.c;
    obj3.d(4000);
    "\b";

    foreach (local x in [obj1, obj2, obj3])
        x.setMethod(&topLoc, m_topLoc), x.setMethod(&getLoc, m_getLoc);
    "obj1.topLoc=<<obj1.topLoc.name>>\n";
    "obj2.getLoc=<<obj2.getLoc.name>>, topLoc=<<obj2.topLoc.name>>\n";
    "obj3.getLoc=<<obj3.getLoc.name>>, topLoc=<<obj3.topLoc.name>>\n";
    "\b";

    obj1.setMethod(&a, fopt);
    obj1.setMethod(&b, { a, b?, c? : a+b+c });
    obj1.setMethod(&c, { a?, ... : a+i });
    obj1.setMethod(
        &d, Compiler.compile('function(a, b, c?) { return a; }'));

    "fopt <<descIfc(getFuncParams(fopt))>>\n";
    "obj1.a <<descMethod(obj1, &a)>>\n";
    "obj1.b <<descMethod(obj1, &b)>>\n";
    "obj1.c <<descMethod(obj1, &c)>>\n";
    "obj1.d <<descMethod(obj1, &d)>>\n";
    "\b";

    obj1.setMethod(&a, &t3GetNamedArg);
    obj1.setMethod(&b, &inputDialog);
    obj1.setMethod(&c, &systemInfo);
    obj1.setMethod(&d, &rexSearch);
    "obj1.a <<descMethod(obj1, &a)>>\n";
    "obj1.b <<descMethod(obj1, &b)>>\n";
    "obj1.c <<descMethod(obj1, &c)>>\n";
    "obj1.d <<descMethod(obj1, &d)>>\n";
    "obj1.d call = <<obj1.d('<alpha>+', '123 test 456', 1)[3]>>\n";
    "\b";

    obj1.ctxTests();
}

descIfc(ifc)
{
    "interface = (minArgc=<<ifc[1]>>, optArgc=<<ifc[2]>>, varargs=<<
      ifc[3] ? 'yes' : 'no'>>)";
}

descMethod(obj, prop)
{
    descIfc(obj.getPropParams(prop));
}

