#include <tads.h>

main(args)
{
    local lst = List.generate({i: i-1}, 52).sort(SortAsc, {a, b: rand(3)-1});

    for (local i in 1..lst.length())
        "<<lst[i]+1>> ";
    "\n";
}
