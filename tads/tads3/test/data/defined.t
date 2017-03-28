class C: object
;

foo: C
;

main(args)
{
    if (defined(foo))
        "foo is defined\n";
    if (defined(foo) && foo.ofKind(C))
        "foo is a C\n";

    if (nil)
        "nil is true???\n";

    if (defined(bar))
        "bar is defined\n";
    if (defined(bar) && bar.ofKind(C))
        "bar is a C\n";

    if (defined(bar) && bar.ofKind(foo))
        "bar is a foo\n";

    "\b";
    local x = __objref(foo, warn);
    local x1 = __objref(foo);
    local y = __objref(bar, warn);
    local y1 = __objref(bar);
    if (x == nil && x == x1)
        "foo is undefined\n";
    else if (defined(foo) && x == foo)
        "foo is defined\n";
    else
        "something is wrong with the foo test\n";
    
    if (y == nil && y == y1)
        "bar is undefined\n";
    else if (defined(bar) && y == bar)
        "bar is defined\n";
    else
        "something is wrong with the bar test\n";
}

