#include "tads.h"
#include "t3.h"
#include "vector.h"

main(args)
{
    local v;
    local x;
    local v2;

    v = new Vector(10);
    x = v;

    "initially empty:\n<<sayVector(v)>>";

    v += 1;
    v += 2;
    v += 3;
    "after adding new members:\n<<sayVector(v)>>";

    v[1] += 30;
    v[2] += 20;
    v[3] += 10;
    "after incrementing members:\n<<sayVector(v)>>";

    "x should not be modified:\n<<sayVector(x)>>";

    v -= 22;
    "after subtracting 22:\n<<sayVector(v)>>";

    v += [50, 60, 70];
    "after adding list:\n<<sayVector(v)>>";

    v -= [31, 50, 70];
    "after subtracting list:\n<<sayVector(v)>>";

    v.insertAt(1, 'a', 'b', 'c');
    "after inserting at element 1:\n<<sayVector(v)>>";

    v.insertAt(2, 'X', 'Y');
    "after inserting at element 2:\n<<sayVector(v)>>";

    v.removeElementAt(3);
    "after removing element 3:\n<<sayVector(v)>>";

    v.removeRange(2, 4);
    "after removing elements 2-4:\n<<sayVector(v)>>";

    v.prepend(999);
    "after prepending 999:\n<<sayVector(v)>>";

    v2 = new Vector(5);
    v2 += 21;
    v2 += 22;
    v2 += 23;

    v += v2;
    "after adding vector:\n<<sayVector(v)>>";

    v2 -= 22;
    v2 -= 23;
    v2 += 'a';
    v -= v2;
    "v2:\n<<sayVector(v2)>>";
    "after subtracting vector v2:\n<<sayVector(v)>>";

    v2.append('b');
    "v2 after append:\n<<sayVector(v2)>>";

    v2.appendAll(['c', 'd', 'e']);
    "v2 after appendAll:\n<<sayVector(v2)>>";

    "\b";
    v = new Vector(5, [1, 2]);
    v += 3;
    x = [4, 5, 6];
    x += v;
    "list + vector:\n<<sayVector(x)>>";

    v = new Vector(5, [8]);
    v += [2, 4, 6];
    x -= v;
    "list - vector:\n<<sayVector(x)>>";

    "\b";
    v = Vector.generate({i: i*10}, 10);
    "Vector.generate multiples of 10:\n<<sayVector(v)>>";

    "\b";
    v.splice(5, 2);
    "splice(5,2): <<sayVectorI(v)>>\n";

    v.splice(-2, 0, 'a', 'b', 'c');
    "splice(-2,0,a,b,c): <<sayVectorI(v)>>\n";

    v.splice(0, 0, 'd', 'e', 'f');
    "splice(0,0,d,e,f): <<sayVectorI(v)>>\n";

    v.splice(-2, 9, 'x');
    "splice(-2,9,x): <<sayVectorI(v)>>\n";

    v.splice(1, 0, 'xxx', 'yyy');
    "splice(1,0,xxx,yyy): <<sayVectorI(v)>>\n";

    "\b";
    "toList(-3): <<sayVectorI(v.toList(-3))>>\n";

    x = Vector.generate({i: i*11}, 10);
    x.copyFrom(v, -3, -5, 3);
    "copyFrom(-3, -5, 3): <<sayVectorI(x)>>\n";

    x.fillValue('fill', -6, 3);
    "fillValue(fill, -6, 3): <<sayVectorI(x)>>\n";

    x.insertAt(-2, 'one', 'two');
    "insertAt(-2, one, two): <<sayVectorI(x)>>\n";

    x.removeElementAt(-3);
    "removeElementAt(-3): <<sayVectorI(x)>>\n";

    x.removeRange(-7, -5);
    "removeRange(-7, -5): <<sayVectorI(x)>>\n";
}

sayVector(v)
{
    for (local i = 1, local cnt = v.length() ; i <= cnt ; ++i)
        "\t[<<i>>] <<v[i]>>\n";
}

sayVectorI(v)
{
    "[";
    for (local i in 1..v.length())
        "<<if i > 1>>, <<end>><<v[i]>>";
    "]";
}
