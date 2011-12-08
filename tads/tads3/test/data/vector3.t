#include <tads.h>

modify Vector
    myjoin()
    {
        local s = '';
        for (local i = 1, local len = length() ; i <= len ; ++i)
        {
            if (i > 1)
                s += ', ';
            s += self[i];
        }
        return s;
    }
;

main(args)
{
    local v;

    v = new Vector();
    "[1] v = [<<v.myjoin()>>], length = <<v.length()>>\n";

    [10, 9, 8, 7, 6, 5].forEach({ x: v.append(x) });
    "[2] v = [<<v.myjoin()>>], length = <<v.length()>>\n";

    v = new Vector(10);
    "[3] v = [<<v.myjoin()>>], length = <<v.length()>>\n";

    [10, 9, 8, 7, 6, 5].forEach({ x: v.append(x) });
    "[4] v = [<<v.myjoin()>>], length = <<v.length()>>\n";

    v = new Vector(['a', 'b', 'c', 'd', 'e']);
    "[5] v = [<<v.myjoin()>>], length = <<v.length()>>\n";

    [10, 9, 8, 7, 6, 5].forEach({ x: v.append(x) });
    "[6] v = [<<v.myjoin()>>], length = <<v.length()>>\n";

    v = new Vector(v);
    "[7] v = [<<v.myjoin()>>], length = <<v.length()>>\n";

    [10, 9, 8, 7, 6, 5].forEach({ x: v.append(x) });
    "[8] v = [<<v.myjoin()>>], length = <<v.length()>>\n";

    v = new Vector(3, v);
    "[9] v = [<<v.myjoin()>>], length = <<v.length()>>\n";
}
