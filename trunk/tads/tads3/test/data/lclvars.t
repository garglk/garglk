#include <tads.h>

main(args)
{
    local i = 1;
    local j = 2;
    local k = 3;

    f(i, j, k);
}

f(a, b, c)
{
    local x = a+b;
    local y = b+c;
    local z = a+c;

    dumpStack('f #1');
    g(x, y, z);

    if (a > 0)
    {
        local aa = a * 2;
        local bb = b * 2;
        local cc = c * 2;
        "f #2: aa=<<aa>>, bb=<<bb>>, cc=<<cc>>\n";
        dumpStack('f #2');
        g(x*100, y*100, z*100);
    }
    dumpStack('f #3');

    x *= 10;
    y *= 10;
    z *= 10;
    dumpStack('f #4');

    local dyn = Compiler.compile(
        'function(cx, cy) { '
        + '"This is DynamicFunc 1 (cx=<\<cx>\>, cy=<\<cy>\>)!\\n";'
        + 'dumpStack(\'DynamicFunc 1\');'
        + '}');

    dyn(101, 202);

    rexReplace('<alpha>+', 'test string',
               { match, idx: dumpStack('rexReplace callback'), '<<match>>' });

    "Trying exception trace...\n";
    try
    {
        x.ofKind(Object);
    }
    catch (Exception exc)
    {
        exc.displayException();
    }

    "\bDone!\n";
}

dumpStack(id)
{
    "Stack trace for <<id>>:\n";
    local t = t3GetStackTrace(nil, T3GetStackLocals | T3GetStackDesc);
    for (local i = 1, local cnt = t.length() ; i <= cnt ; ++i)
    {
        local ti = t[i];
        "\t<<reflectionServices.formatStackFrame(ti, nil)>>";
        if (ti.frameDesc_ != nil)
            " [invokee=<<
              reflectionServices.valToSymbol(ti.frameDesc_.getInvokee())>>]";

        if (ti.locals_ != nil)
        {
            "\n\tLocals via T3GetStackLocals:\n";
            ti.locals_.forEachAssoc(
                { key, val: "\t\t<<key>> = <<
                      reflectionServices.valToSymbol(val)>>\n" });
        }
        else
            "\n\t(no locals)\n";

        if (ti.frameDesc_ != nil)
        {
            "\n\tLocals via StackFrameDesc.getVars():\n";
            ti.frameDesc_.getVars().forEachAssoc(
                { key, val: "\t\t<<key>> = <<
                      reflectionServices.valToSymbol(val)>>\n" });
        }
        else
            "\n\t(no frame ref)\n";
    }
    
    "\b";
}

dumpVal(val)
{
    switch (dataType(val))
    {
    case TypeSString:
        "'<<val.htmlify>>'";
        return;
        
    case TypeInt:
        "<<val>>";
        return;

    case TypeObject:
    case TypeBifPtr:
    case TypeProp:
    case TypeEnum:
        tadsSay(reflectionServices.valToSymbol(val));
        return;

    case TypeList:
        "[";
        for (local i = 1, local len = val.length() ; i <= len ; ++i)
        {
            if (i > 1)
                ", ";
            dumpVal(val[i]);
        }
        "]";
        return;

    case TypeTrue:
        "true";
        return;

    case TypeNil:
        "nil";
        return;

    default:
        "(other)";
        return;
    }
}
