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
}

sayVector(v)
{
    for (local i = 1, local cnt = v.length() ; i <= cnt ; ++i)
        "\t[<<i>>] <<v[i]>>\n";
}
