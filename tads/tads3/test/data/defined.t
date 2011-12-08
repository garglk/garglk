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
}

