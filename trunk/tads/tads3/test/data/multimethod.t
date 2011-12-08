#charset "cp1252"
#include <tads.h>

class Thing: object;
class Table: Thing;
class DiningTable: Table;

main(args)
{
    "Accented characters: Ññ\n";

    "f(A, B):\n";
    f(aObj, bObj);

    "\bf(B, A):\n";
    f(bObj, aObj);

    "\bf(C, D):\n";
    f(cObj, dObj);

    "\bf(D, C):\n";
    f(dObj, cObj);

    "\bf(A):\n";
    f(aObj);

    "\bf(B):\n";
    f(bObj);

    "\bf(C):\n";
    f(cObj);

    "\bf():\n";
    f();

    "\bf(A, 1):\n";
    f(aObj, 1);

    "\bf(A, B, 2):\n";
    f(aObj, bObj, 2);
    
    local x = f;
    "\bx(C, D):\n";
    x(cObj, dObj);

    local v = new Vector(5);
    v.append('One');
    v.append('Two');
    local s = 'Test String';
    local l = ['Test', 'List'];

    "\bg(String, List):\n";
    g(s, l);

    "\bg(String, Vector, Vector):\n";
    g(s, v, v);

    "\bg(List, Vector, Vector):\n";
    g(l, v, v);

    x = new Table();

    "\bh1(Table, Table):\n";
    h1(x, x);

    "\bh2(Table, Table):\n";
    h2(x, x);

    x = new DiningTable();

    "\bh3(DiningTable, DiningTable):\n";
    h3(x, x);

    "\bh4(DiningTable, DiningTable):\n";
    h4(x, x);

    "\bkf(cObj, cObj):\n";
    kf(cObj, cObj);
}

h1(Thing a, Thing b) { "h1.A\n"; }
h1(Thing a, Table b) { "h1.B\n"; }
h1(Table a, Thing b) { "h1.C\n"; }

h2(Thing a, Thing b) { "h2.A\n"; }
h2(Thing a, Table b) { "h2.B\n"; }
h2(Table a, Thing b) { "h2.C\n"; }
h2(Table a, Table b) { "h2.D\n"; }

h3(Thing a, Thing b) { "h3.A\n"; }
h3(Table a, Thing b) { "h3.B\n"; }
h3(Thing a, Table b) { "h3.C\n"; }
h3(Thing a, DiningTable b) { "h3.D\n"; }

h4(Thing a, Thing b) { "h4.A\n"; }
h4(Table a, Thing b)
{
    "h4.B - inheriting A (Thing, Thing)...\n";
    inherited<Thing, Thing>(a, b);

    "h4.B - inheriting next...\n";
    inherited(a, b);
}
h4(Table a, Table b)
{
    "h4.C - inheriting B (Table, Thing)...\n";
    inherited<Table, Thing>(a, b);

//    "\bh4.C - inheriting A (Thing,Table)...\n";
//    inherited<Thing, Table>(a, b);

    "\bh4.C - inheriting A (Thing,Thing)...\n";
    inherited<Thing, Thing>(a, b);

    "\bh4.C - inheriting next...\n";
    inherited(a, b);

//    mminherited(h2 a, a b);
//    mminherited(Thing a, b);
}

g(String s, List l)
{
    "g(String(<<s>>), List)\n";
}
g(String s, Vector v, Vector w)
{
    "g(String(<<s>>), Vector, Vector)\n";
}
g(List l, Vector v, Vector w)
{
    "g(List, Vector, Vector)\n";
}

class A: object
    name = 'A'
;

class B: A
    name = 'B'
;

class C: B
    name = 'C'
;

class D: B
    name = 'D'
;

aObj: A;
bObj: B;
cObj: C;
dObj: D;

f(A x, A y)
{
    "f(A,A)\n";
    "Inheriting f(A, ...)...\n";
    inherited<A, ...>(x, y);

    "Inheriting...\n";
    inherited(x, y);
}

f(B x, B y)
{
    "f(B,B)\n";
}

f(C x, D y)
{
    "f(C,D)\n";
}

f(B b)
{
    "f(B)\n";
    "inheriting f(*)...\n";
    inherited<*>(b);
}

f(x) multimethod
{
    "f(x)\n";

    "inheriting f()...\n";
    inherited<>();
}

f() multimethod
{
    "f()\n";
}

f(A a, [args])
{
    "f(A,...)\n";
}

kf(A a1, A a2) { "\nf(A a1=<<a1.name>>, A a2=<<a2.name>>) "; }
kf(A a1, B b1) { "\nf(A a1=<<a1.name>>, B b1=<<b1.name>>) "; inherited(a1, b1); }
kf(A a1, C c1) { "\nf(A a1=<<a1.name>>, C c1=<<c1.name>>) "; inherited(a1, c1); }
kf(B b1, A a1) { "\nf(B b1=<<b1.name>>, A a1=<<a1.name>>) "; inherited(b1, a1); }
kf(B b1, B b2) { "\nf(B b1=<<b1.name>>, B b2=<<b2.name>>) "; inherited(b1, b2); }
kf(B b1, C c1) { "\nf(B b1=<<b1.name>>, C c1=<<c1.name>>) "; inherited(b1, c1); }
kf(C c1, A a1) { "\nf(C c1=<<c1.name>>, A a1=<<a1.name>>) "; inherited(c1, a1); }
kf(C c1, B b1) { "\nf(C c1=<<c1.name>>, B b1=<<b1.name>>) "; inherited(c1, b1); }
kf(C c1, C c2) { "\nf(C c1=<<c1.name>>, C c2=<<c2.name>>) "; inherited(c1, c2); }

replace kf(B b1, B b2)
{
    "\n!!Replaced!! f(B b1=<<b1.name>>, B b2=<<b2.name>>) ";
    inherited(b1, b2);
}

modify kf(A a1, B b1)
{
    "\n!!Modified!! f(A a1=<<a1.name>>, B b1=<<b1.name>>) ";

    "\n[ inherited:";
    inherited(a1, b1);
    "\n..back from inherited!]";

    "\n[ replaced:";
    replaced(a1, b1);
    "\n.. back from replaced!]";
}
