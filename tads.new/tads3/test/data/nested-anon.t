#include <tads.h>

main(args)
{
    local v = new Vector(10);
    
    for (local i = 1 ; i <= 10 ; ++i)
        v.append({x: {: x } }(i));

    for (local i = 1 ; i <= 10 ; ++i)
        "(v[<<i>>])() = <<(v[i])()>>\n";
}
