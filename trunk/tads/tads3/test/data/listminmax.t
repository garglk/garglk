#include <tads.h>

main(args)
{
    test(['xx']);
    test(['a', 'bb', 'ccc', 'dddd', 'eeeee']);
    test(['eeeee', 'dddd', 'ccc', 'bb', 'a']);
    test(['aaaaa', 'bbbb', 'ccc', 'dd', 'e']);
    test(['e', 'dd', 'ccc', 'bbbb', 'aaaaa']);
    test(['eE', 'ddDD', 'cccCCC', 'bbbbBBBB', 'aaaaaAAAAA']);
}

test(a)
{
    "*****\n";
    "as list:\b";
    test1(a);

    "=====\n";
    "as vector:\b";
    test1(new Vector(a));
}

test1(a)
{
    "-----\n";
    "a = [<<a.join(', ')>>]\n";
    "\b";
    
    "min(a) = <<min(a)>>\n";
    "max(a) = <<max(a)>>\n";
    "\b";

    "a.indexOfMin() = <<a.indexOfMin()>>\n";
    "a.minVal() = <<a.minVal()>>\n";
    "a.indexOfMax() = <<a.indexOfMax()>>\n";
    "a.maxVal() = <<a.maxVal()>>\n";
    "\b";

    "a.indexOfMin(length) = <<a.indexOfMin({x: x.length()})>>\n";
    "a.minVal(length) = <<a.minVal({x: x.length()})>>\n";
    "a.indexOfMax(length) = <<a.indexOfMax({x: x.length()})>>\n";
    "a.maxVal(length) = <<a.maxVal({x: x.length()})>>\n";
    "\b";
}
