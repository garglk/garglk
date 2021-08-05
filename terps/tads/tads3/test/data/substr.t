#include <tads.h>

main(args)
{
    test('abcdefghijkl', 0, 100);
    test('abcdefghijkl', -5, 100);
    test('abcdefghijkl', 5, 100);
    test('abcdefghijkl', 5, -100);
    test('abcdefghijkl', 5, -3);
    test('abcdefghijkl', -6, -3);
    test('abcdefghijkl', -1, -1);
    test('abcdefghijkl', -1, -1);
    test('abcdefghijkl', -2, -1);
    test('abcdefghijkl', -3, -1);
    test('abcdefghijkl', -4, -1);
    test('abcdefghijkl', -5, -1);
    test('abcdefghijkl', -6, -1);
    test('abcdefghijkl', -7, -1);
    test('abcdefghijkl', -8, -1);
    test('abcdefghijkl', -9, -1);
    test('abcdefghijkl', -10, -1);
    test('abcdefghijkl', -10, -2);
    test('abcdefghijkl', -10, -3);
    test('abcdefghijkl', -10, -4);
    test('abcdefghijkl', -10, -5);
    test('abcdefghijkl', -10, -6);
    test('abcdefghijkl', -10, -7);
    test('abcdefghijkl', -10, -8);
    test('abcdefghijkl', -10, -9);
    test('abcdefghijkl', -10, -10);
    test('abcdefghijkl', -10, -11);
    test('abcdefghijkl', -11, -1);
    test('abcdefghijkl', -12, -1);
    test('abcdefghijkl', -13, -1);
    test('abcdefghijkl', -14, -1);
    test('abcdefghijkl', -14, -2);
    test('abcdefghijkl', -14, -10);
    test('abcdefghijkl', -14, -11);
    test('abcdefghijkl', -14, -12);
    test('abcdefghijkl', -14, -13);
    test('abcdefghijkl', -14, -14);
    test('abcdefghijkl', 10, -1);
    test('abcdefghijkl', 11, -1);
    test('abcdefghijkl', 12, -1);
    test('abcdefghijkl', 13, -1);
    test('abcdefghijkl', 14, -1);
}

test(str, start, len)
{
    "'<<str>>'.substr(<<start>>, <<len>>) = '<<str.substr(start, len)>>'\n";
}

