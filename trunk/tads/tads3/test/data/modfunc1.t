main(args)
{
    "This is main...\n";
    func('hello'); "\b";
    g(111); "\b";
    "calling h(1000):\n";
    local res = h(1000);
    "-> return value = <<res>>\n";
    "\b";
}

/*
 *   test 1 - function with local modify 
 */
func(x)
{
    "this is the original func(<<x>>)\n";
}

modify func(x)
{
    "this is the modified func(<<x>>)\n";
    replaced(x + '[1]');
}

/*
 *   test 2 - function with no local modify 
 */
g(x)
{
    "this is the original g(<<x>>)\n";
}

/*
 *   test 3 - function with local modify 
 */
h(x)
{
    "this is the original h(<<x>>)\n";
    return x + 5;
}

modify h(x)
{
    "this is the modified h(<<x>>)\n";
    return replaced(x * 2);
}
