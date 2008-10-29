/*
 *   test of <> operator - the compiler explicitly flags this with an error
 */
main(args)
{
    if (args.length <> 1)
        "No arguments allowed!\n";
}
