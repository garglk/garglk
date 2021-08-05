#include <tads.h>

obj: object
    tab = ['one' -> 123, 'two' -> 234, 'three' -> 345, 'four' -> 456]
    showTab()
    {
        local t = tab;
        "t[one] = <<t['one']>>, t[two] = <<t['two']>>,
        t[three] = <<t['three']>>, t[four] = <<t['four']>>\n";
        t.forEachAssoc({ key, val: ".. t[<<key>>] = <<val>>\n" });
    }
;

main(args)
{
    local t = ['one' -> 111, 'two' -> 222, 'three' -> 333, 'four' -> 444];
    "t[one] = <<t['one']>>, t[two] = <<t['two']>>,
    t[three] = <<t['three']>>, t[four] = <<t['four']>>\n";
    t.forEachAssoc({ key, val: ".. t[<<key>>] = <<val>>\n" });
    "\b";

    local v = new Vector(['one', 'A', 'two', 'B', 'three', 'C', 'four', 'D']);
    t = new LookupTable(v);
    "t[one] = <<t['one']>>, t[two] = <<t['two']>>,
    t[three] = <<t['three']>>, t[four] = <<t['four']>>\n";
    t.forEachAssoc({ key, val: ".. t[<<key>>] = <<val>>\n" });
    "\b";

    obj.showTab();
}
