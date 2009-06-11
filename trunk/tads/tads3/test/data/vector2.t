#include "tads.h"
#include "t3.h"
#include "vector.h"

preinit()
{
}

main(args)
{
    local x, y, z;
    local fib0, fib1;

    x = new Vector(7, 7).fillValue('A');
    "create and fill:\n"; showVector(x);

    x = new Vector(0, [10, 9, 8, 7, 6, 5, 4, 3, 2, 1]);
    "initially: x = \n"; showVector(x);
    "\b";

    y = x;
    y[3] = 100;
    "after modifying through reference: y = \n"; showVector(y);
    "x = \n"; showVector(x);
    "\b";

    y = x.toList();
    "toList:\n"; showList(y);
    "\b";

    y = x.toList(5);
    "toList(5):\n"; showList(y);
    "\b";

    y = x.toList(17);
    "toList(17):\n"; showList(y);
    "\b";

    y = x.toList(15, 17);
    "toList(15, 17):\n"; showList(y);
    "\b";    

    y = x.toList(3, 5);
    "toList(3, 5):\n"; showList(y);
    "\b";

    y = x.toList(10, 8);
    "toList(10, 8):\n"; showList(y);
    "\b";

    y = new Vector(0, x);
    "direct copy:\n"; showVector(y);
    "\b";

    y.copyFrom(['a', 'b', 'c', 'd'], 1, 3, 7);
    "copyFrom 1,3,7:\n"; showVector(y);
    "\b";

    y = new Vector(0, x);
    y.copyFrom(['a', 'b', 'c', 'd'], 1, 7, 7);
    "copyFrom 1,7,7:\n"; showVector(y);
    "\b";

    y = new Vector(0, x);
    y.copyFrom(['a', 'b', 'c', 'd'], 1, 4, 3);
    "copyFrom 1,4,3:\n"; showVector(y);
    "\b";

    y = new Vector(0, x);
    y.copyFrom(['a', 'b', 'c', 'd'], 3, 5, 7);
    "copyFrom 3,5,7:\n"; showVector(y);
    "\b";

    y = new Vector(10, 10);
    y.fillValue('x');
    "fillValue 'x':\n"; showVector(y);
    "\b";

    y = new Vector(10, 10);
    y.fillValue(123, 3);
    "fillValue 123, 3:\n"; showVector(y);
    "\b";

    y = new Vector(10, 10);
    y.fillValue('abc', 3, 4);
    "fillValue 'abc', 3, 4:\n"; showVector(y);
    "\b";

    fib0 = 1;
    fib1 = 1;
    y = new Vector(20, 20).applyAll(new function(x)
        { local ret = fib0; fib0 = fib1; fib1 += ret; return ret; });
    "applyAll fibonacci:\n"; showVector(y);
    "\b";

    "Subset >= 10:\n"; showVector(y.subset({x: x >= 10}));
    "\b";

    "find where x > 10:  <<y.indexWhich({x: x > 10})>>\n";
    "find where x > 100:  <<y.indexWhich({x: x > 100})>>\n";
    "find where x > 10000:  <<y.indexWhich({x: x > 10000})>>\n";
    "\b";

    x = y.mapAll({x: x + 1});
    "mapAll plus one:\n"; showVector(x);
    "mapAll == orig? <<x == y ? 'yes' : 'no'>>\n";
    "mapAll original:\n"; showVector(y);
    "\b";

    "indexOf 1: <<y.indexOf(1)>>\n";
    "indexOf 54: <<y.indexOf(54)>>\n";
    "indexOf 55: <<y.indexOf(55)>>\n";
    "indexOf 6765: <<y.indexOf(6765)>>\n";
    "\b";

    "lastIndexOf 1: <<y.lastIndexOf(1)>>\n";
    "lastIndexOf 54: <<y.lastIndexOf(54)>>\n";
    "lastIndexOf 55: <<y.lastIndexOf(55)>>\n";
    "lastIndexOf 6765: <<y.lastIndexOf(6765)>>\n";
    "\b";

    "lastIndexWhich(x &lt; 1): <<y.lastIndexWhich({x: x < 1})>>\n";
    "lastIndexWhich(x &lt; 2): <<y.lastIndexWhich({x: x < 2})>>\n";
    "lastIndexWhich(x &lt; 10): <<y.lastIndexWhich({x: x < 10})>>\n";
    "lastIndexWhich(x &lt; 100): <<y.lastIndexWhich({x: x < 100})>>\n";
    "lastIndexWhich(x &lt; 10000): <<y.lastIndexWhich({x: x < 10000})>>\n";
    "\b";

    "lastValWhich(x &lt; 1): <<y.lastValWhich({x: x < 1})>>\n";
    "lastValWhich(x &lt; 2): <<y.lastValWhich({x: x < 2})>>\n";
    "lastValWhich(x &lt; 10): <<y.lastValWhich({x: x < 10})>>\n";
    "lastValWhich(x &lt; 100): <<y.lastValWhich({x: x < 100})>>\n";
    "lastValWhich(x &lt; 10000): <<y.lastValWhich({x: x < 10000})>>\n";
    "\b";

    "countOf 0: <<y.countOf(0)>>\n";
    "countOf 1: <<y.countOf(1)>>\n";
    "countOf 55: <<y.countOf(55)>>\n";
    "\b";

    "countWhich x>10: <<y.countWhich({x: x > 10})>>\n";
    "countWhich x>100: <<y.countWhich({x: x > 100})>>\n";
    "countWhich x>1000: <<y.countWhich({x: x > 1000})>>\n";
    "countWhich x>10000: <<y.countWhich({x: x > 10000})>>\n";
    "\b";

    x = new Vector(0, [2, 4, 6, 8, 10, 12]);
    "initially: x = \n"; showVector(x);
    "getUnique:\n"; showVector(x.getUnique());

    x = new Vector(0, [9, 1, 9, 2, 9, 3, 9, 2, 9, 1, 9]);
    "initially: x = \n"; showVector(x);
    "getUnique:\n"; showVector(x.getUnique());

    "\b";

    x = new Vector(0, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    y = new Vector(0, [2, 4, 6, 8, 10, 12, 14, 16, 18, 20]);
    "appendUnique:\n"; showVector(x.appendUnique(y));

    x = new Vector(0, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    y = new Vector(0, [11, 12, 13, 14, 15]);
    "appendUnique:\n"; showVector(x.appendUnique(y));

    x = new Vector(0, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    y = new Vector(0, [2, 4, 6, 8, 10]);
    "appendUnique:\n"; showVector(x.appendUnique(y));

    x = new Vector(0, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    "appendUnique - list:\n";
    "appendUnique:\n"; showVector(x.appendUnique([5, 10, 15, 20]));

    "\b";

    y = new Vector(0, ['hello', 'there', 'how', 'are', 'you', 'today']);
    "sort:\n"; showVector(y.sort());

    "sort descending:\n"; showVector(y.sort(true));

    "\b";

    x = new Vector(0, [1, 1, 2, 3, 5, 8, 13]);
    y = [101, 102, 103];
    z = x + y;
    "adding list to vector:\n"; showVector(z);

    y = new Vector(5);
    y += [987, 654, 321];
    z = x + y;
    "adding vector to vector:\n"; showVector(z);

    y = new Vector(0, [111, 222, 333]);
    z = x + y;
    "adding vector to vector:\n"; showVector(z);

    y = [2, 3, 5, 8];
    z = x - y;
    "subtracting list from vector:\n"; showVector(z);

    y = new Vector(5);
    y += [1, 13];
    z = x - y;
    "subtracting vector from vector:\n"; showVector(z);

    y = new Vector(0, [1, 3, 5]);
    z = x - y;
    "subtracting vector from vector:\n"; showVector(z);
}

showVector(x)
{
    for (local i = 1 ; i <= x.length() ; ++i)
        "\t[<<i>>] = <<showVal(x[i])>>\n";
}

showList(x)
{
    for (local i = 1 ; i <= x.length() ; ++i)
        "\t[<<i>>] = <<showVal(x[i])>>\n";
}

showVal(x)
{
    if (x == nil)
        "nil";
    else
        tadsSay(x);
}

