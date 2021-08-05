#include <tads.h>

main(args)
{
    local lst = List.generate({i: i*2}, 10);

    "for..in:\n";
    for (local ele in lst)
        "<<ele>>\n";

    "\b";
    "for..in with a counter:\n";
    for (local ele in lst, local i = 1 ; ; ++i)
        "lst[<<i>>] = <<ele>>\n";

    "\b";
    "for..in range:\n";
    for (local i in 1..lst.length())
        "lst[<<i>>] = <<lst[i]>>\n";

    "\b";
    "for..in range, with positive step:\n";
    for (local i in 1..lst.length() step 2)
        "lst[<<i>>] = <<lst[i]>>\n";

    "\b";
    "for..in range, with negative step:\n";
    for (local i in lst.length()..1 step -1)
        "lst[<<i>>] = <<lst[i]>>\n";

    "\b";
    "for..in range, with positive variable step:\n";
    local s = 1;
    for (local i in 1..lst.length() step s*3)
        "lst[<<i>>] = <<lst[i]>>\n";

    "\b";
    "for..in range, with negative variable step:\n";
    s = -1;
    for (local i in lst.length()..1 step s*4)
        "lst[<<i>>] = <<lst[i]>>\n";

    "\b";
    "for..in range, with BigNumber step:\n";
    for (local i in 1.0..2.0 step 0.1)
        "i=<<i>>\n";

    "\b";
    "for..in range, with negative BigNumber step:\n";
    for (local i in 5.5..4.4 step -0.2)
        "i=<<i>>\n";
}
