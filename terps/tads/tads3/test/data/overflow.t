#include <tads.h>

myFunc(x) { return x + 1; }
ident(x) { return x; }

ident2(...) { return argcount; }

main(args)
{
    local a = 2000000000;
    local b = 2000000000;

    "a=<<a>>, b=<<b>>, a*b=<<a*b>>\n";

    a = -8;
    b = 3;
    "a=<<a>>, b=<<b>>, a/b=<<a/b>>\n";

    a = myFunc;
    b = a(5);
    "myFunc: b=<<b>>\n";

    a = 17.000;
    b = 17;
    "a=<<a>>, b=<<b>>, a==b=<<a==b ? 'true' : 'nil'>>\n";

    a = 181;
    b = 181.000;
    "a=<<a>>, b=<<b>>, a==b=<<a==b ? 'true' : 'nil'>>\n";

    a = [1, 2, 3];
    a[1] = 10;
    ident(a)[2] = 20;
    "a=[ ";
    foreach (local x in a) "<<x>> ";
    "]\n";

    "ident2(a,b) = <<ident2(a,b)>>\n";

    for (a = 1 ; a <= 10 ; ++a)
    {
        local j = 3, k = 5;

        if (a == 1)
            j = 10;

        "loop: a=<<a>>, j=<<j>>\n";
    }

    local h = new LookupTable(10, 20);
    h[10] = 'ten';
    h[11] = 'eleven';
    h[12] = 'twelve';
    foreach (a in h)
        "foreach: <<a>>\n";

    try
    {
        "\bThis is the try!\n";
        throw Exception;
    }
    catch (Exception e)
    {
        "This is the exception handler!\n";
        goto done1;
    }
    finally
    {
        "This is the finally block!\n";
    }
    
done1:
    "This is done1!\n";

    try
    {
        "\bThis is try #2 outer!\n";
        
        try
        {
            "This is try #2 inner!\n";
            throw Exception;
        }
        catch (Exception e)
        {
            "This catch #2 inner!\n";
            throw e;
        }
        finally
        {
            "This is the inner #2 finally block!\n";
        }
    }
    catch (Exception e)
    {
        "This is catch #2 outer!\n";
    }
    finally
    {
        "This is the outer #2 finally block\n";
    }

    "\^<a href='test'><i>hello from caps-flag html test!</i></a>\n";
}
