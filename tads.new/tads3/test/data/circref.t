#include <tads.h>
#include <vector.h>

main(args)
{
    local a, b;

    a = new Vector(1);
    b = new Vector(1);
    "empty: a == b? <<a == b ? 'yes' : 'no'>>\n";

    a.append(b);
    "a = [b], b = []: a == b ? <<a == b ? 'yes' : 'no'>>\n";
    
    b.append(a);
    a = a.toList();
    b = b.toList();
    "a = [b], b = [a]: a == b? <<a == b ? 'yes' : 'no'>>\n";
}
