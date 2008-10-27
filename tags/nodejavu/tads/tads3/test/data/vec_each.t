#include <tads.h>
#include <vector.h>

main(args)
{
    local v = new Vector(5);
    local cnt;

    v += 0;
    v += 0;
    v += 0;
    v += 0;
    v += 0;
    cnt = 0;
    v.applyAll({x: cnt++});
    foreach (local i in v)
        "<< i >>\n";
}

