/*
 *   test 1 - modify external function twice; base function is itself modified
 */
modify func(x)
{
    "this is the first external modified func(<<x>>)\n";
    replaced(x + '[2]');
}

modify func(x)
{
    "this is the second external modified func(<<x>>)\n";
    replaced(x + '[3]');
}

/* 
 *   test 2 - modify external function once; base function is not modified
 */
modify g(x)
{
    "this is the external g(<<x>>)\n";
    replaced(x * 2);
}

/*
 *   test 3 - modify external function once; base function is itself modified
 */
modify h(x)
{
    "this is the external h(<<x>>)\n";
    return replaced(x * 2);
}

