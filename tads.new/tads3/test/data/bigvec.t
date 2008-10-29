#include <tads.h>
#include <vector.h>

main(args)
{
    testVec(32767);
    testVec(255*255);
    testVec(65500);
    testVec(65535);
    testVec(65536);
    testVec(100000);
}

testVec(len)
{
    local v;

    "new(<<len>>)... ";
    
    try
    {
        v = new Vector(len);
    }
    catch (Exception exc)
    {
        "*** allocating: caught exception: <<exc.displayException()>>\n";
        return;
    }

    try
    {
        v[1] = 'first';
        v[len] = 'last';
        "v[1] = <<v[1]>>; v[<<len>>] = <<v[len]>>\n";
    }
    catch (Exception exc)
    {
        "*** indexing: caught exception: <<exc.displayException()>>\n";
    }
}
