/*
 *   Another address operator test.  This tests what happens if we use "&f"
 *   and then define "f" as a function later in the same file. 
 */

#include <tads.h>

main(args)
{
    local a = &f;
    "a=&amp;f: dataType(a)=<<dataType(a)>>, a(17)=<<a(17)>>\n";
}

f(x)
{
    return x*2;
}

