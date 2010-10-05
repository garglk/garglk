#include <tads.h>

enum x, y, z;
o: object
    p1 = 1
    p2 = 2
;
//x: object; y: object; z: object;
main(args)
{
    local vec, cnt = 0, p1 = 7;
//    vec = new Vector(3, [x, y, z]);
    vec = [x, y, z];
    
    vec.mapAll(new function(val)
    {
        cnt += p1;
#if 0
        if (val == x) "[x]";
        else if (val == y ) "[y]";
        else if (val == z) "[z]";
        else "[?]";
#else    
        switch(val)
        {
            case x:
                "x ";
                break;
 
            case y:
                "y ";
                break;
 
            case z:
                "z ";
                break;
 
            default:
                "? ";
        }
#endif
    });
    "\ncnt = <<cnt>>\n";
}
