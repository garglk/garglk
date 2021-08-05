#error THIS FILE IS NOW OBSOLETE - DO NOT USE.

#include "tads.h"
#include "t3.h"
#include "array.h"
#include "vector.h"

preinit()
{
}

main(args)
{
    local x, y, z;
    local fib0, fib1;

    x = new Array(7).fillValue('A');
    "create and fill:\n"; showArray(x);

    x = new Array([10, 9, 8, 7, 6, 5, 4, 3, 2, 1]);
    "initially: x = \n"; showArray(x);
    "\b";

    y = x;
    y[3] = 100;
    "after modifying through reference: y = \n"; showArray(y);
    "x = \n"; showArray(x);
    "\b";

    y = new Array(x, 1, 20);
    "new Array(x, 1, 20):\n"; showArray(y);
    "\b";

    y = new Array(x, 1, 5);
    "new Array(x, 1, 5):\n"; showArray(y);
    "\b";

    y = new Array(x, 3, 5);
    "new Array(x, 3, 5):\n"; showArray(y);
    "\b";

    y = new Array(x, 5, 20);
    "new Array(x, 5, 20):\n"; showArray(y);
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

    y = new Array(x);
    "direct copy:\n"; showArray(y);
    "\b";

    y.copyFrom(['a', 'b', 'c', 'd'], 1, 3, 7);
    "copyFrom 1,3,7:\n"; showArray(y);
    "\b";

    y = new Array(x);
    y.copyFrom(['a', 'b', 'c', 'd'], 1, 7, 7);
    "copyFrom 1,7,7:\n"; showArray(y);
    "\b";

    y = new Array(x);
    y.copyFrom(['a', 'b', 'c', 'd'], 1, 4, 3);
    "copyFrom 1,4,3:\n"; showArray(y);
    "\b";

    y = new Array(x);
    y.copyFrom(['a', 'b', 'c', 'd'], 3, 5, 7);
    "copyFrom 3,5,7:\n"; showArray(y);
    "\b";

    y = new Array(10);
    y.fillValue('x');
    "fillValue 'x':\n"; showArray(y);
    "\b";

    y = new Array(10);
    y.fillValue(123, 3);
    "fillValue 123, 3:\n"; showArray(y);
    "\b";

    y = new Array(10);
    y.fillValue('abc', 3, 4);
    "fillValue 'abc', 3, 4:\n"; showArray(y);
    "\b";

    fib0 = 1;
    fib1 = 1;
    y = new Array(20).applyAll(new function(x)
        { local ret = fib0; fib0 = fib1; fib1 += ret; return ret; });
    "applyAll fibonacci:\n"; showArray(y);
    "\b";

    "Subset >= 10:\n"; showArray(y.subset({x: x >= 10}));
    "\b";

    "find where x > 10:  <<y.indexWhich({x: x > 10})>>\n";
    "find where x > 100:  <<y.indexWhich({x: x > 100})>>\n";
    "find where x > 10000:  <<y.indexWhich({x: x > 10000})>>\n";
    "\b";

    x = y.mapAll({x: x + 1});
    "mapAll plus one:\n"; showArray(x);
    "mapAll == orig? <<x == y ? 'yes' : 'no'>>\n";
    "mapAll original:\n"; showArray(y);
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

    "lastIndexWhich(x < 1): <<y.lastIndexWhich({x: x < 1})>>\n";
    "lastIndexWhich(x < 2): <<y.lastIndexWhich({x: x < 2})>>\n";
    "lastIndexWhich(x < 10): <<y.lastIndexWhich({x: x < 10})>>\n";
    "lastIndexWhich(x < 100): <<y.lastIndexWhich({x: x < 100})>>\n";
    "lastIndexWhich(x < 10000): <<y.lastIndexWhich({x: x < 10000})>>\n";
    "\b";

    "lastValWhich(x < 1): <<y.lastValWhich({x: x < 1})>>\n";
    "lastValWhich(x < 2): <<y.lastValWhich({x: x < 2})>>\n";
    "lastValWhich(x < 10): <<y.lastValWhich({x: x < 10})>>\n";
    "lastValWhich(x < 100): <<y.lastValWhich({x: x < 100})>>\n";
    "lastValWhich(x < 10000): <<y.lastValWhich({x: x < 10000})>>\n";
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

    x = new Array([2, 4, 6, 8, 10, 12]);
    "initially: x = \n"; showArray(x);
    "getUnique:\n"; showArray(x.getUnique());

    x = new Array([9, 1, 9, 2, 9, 3, 9, 2, 9, 1, 9]);
    "initially: x = \n"; showArray(x);
    "getUnique:\n"; showArray(x.getUnique());

    "\b";

    x = new Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    y = new Array([2, 4, 6, 8, 10, 12, 14, 16, 18, 20]);
    "appendUnique:\n"; showArray(x.appendUnique(y));

    y = new Array([11, 12, 13, 14, 15]);
    "appendUnique:\n"; showArray(x.appendUnique(y));

    y = new Array([2, 4, 6, 8, 10]);
    "appendUnique:\n"; showArray(x.appendUnique(y));

    "appendUnique - list:\n";
    "appendUnique:\n"; showArray(x.appendUnique([5, 10, 15, 20]));

    "\b";

    y = new Array(['hello', 'there', 'how', 'are', 'you', 'today']);
    "sort:\n"; showArray(y.sort());

    "sort descending:\n"; showArray(y.sort(true));

    "\b";

    x = new Array([1, 1, 2, 3, 5, 8, 13]);
    y = [101, 102, 103];
    z = x + y;
    "adding list to array:\n"; showArray(z);

    y = new Vector(5);
    y += [987, 654, 321];
    z = x + y;
    "adding vector to array:\n"; showArray(z);

    y = new Array([111, 222, 333]);
    z = x + y;
    "adding array to array:\n"; showArray(z);

    y = [2, 3, 5, 8];
    z = x - y;
    "subtracting list from array:\n"; showArray(z);

    y = new Vector(5);
    y += [1, 13];
    z = x - y;
    "subtracting vector from array:\n"; showArray(z);

    y = new Array([1, 3, 5]);
    z = x - y;
    "subtracting array from array:\n"; showArray(z);
}

showArray(x)
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

