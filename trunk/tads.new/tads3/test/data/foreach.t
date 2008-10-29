#include "tads.h"
#include "t3.h"
#include "vector.h"

main(args)
{
    local i, vec;
    
    args = ['hello', 'there', 'how', 'are', 'you', 'today'];

    "Constant list:\n";
    foreach(local x in args)
        "<<x>>\n";
    "\b";

    args += '!!!';
    "Non-constant list:\n";
    foreach (i in args)
        "<<i>>\n";
    "\b";

    i = 1;
    vec = new Vector(10).setLength(10).applyAll({x: i++});

    "Vector:\n";
    foreach(i in vec)
        "<<i>>\n";
    "\b";

    "Vector, modifying throughout foreach:\n";
    i = 1;
    foreach(local x in vec)
    {
        vec.applyAll({v: v+1});
        "x = <<x>>, vec[<<i>>] = <<vec[i]>>\n";
        ++i;
    }
    "\b";
}
