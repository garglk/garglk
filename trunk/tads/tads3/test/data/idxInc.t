/*
 *   test of code generation for assignments to index expressions with side
 *   effects
 */

#include <tads.h>

main(args)
{
    local v = new Vector(5, [10, 20, 30, 40]);
    local i = 1;

    "before: i=<<i>>, v=<<showVec(v)>>\n";
    v[i++]++;
    "after: i=<<i>>, v=<<showVec(v)>>\n";
}

showVec(v)
{
    for (local i = 1, local len = v.length() ; i <= len ; ++i)
    {
        if (i > 1)
            ", ";
        tadsSay(v[i]);
    }
}
