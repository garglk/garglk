#include <tads.h>

main(args)
{
    test([1, 2, 'buckle my shoe', 3.00, 4.0000, 'open the door']);
    test([1, 2, 'buckle my shoe', 3.00, 4.0000, 'open the door'], ',');
    test([1, 2, 'buckle my shoe', 3.00, 4.0000, 'open the door'], ', ');

    local v = new Vector();
    v.append(1);
    v.append('hello');
    v.append(7.8);
    v[3] *= 3.14159265;
    v.append('another string');
    test(v);
    test(v, '; ');
}

test(lst, sep?)
{
    "<<lst.join(sep).htmlify()>><br>";
}
