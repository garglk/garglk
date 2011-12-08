#include <tads.h>

main(args)
{
    local t = ['one' -> 111, 'two' -> 222, 'three' -> 333, 'four' -> 444];
    "t[one] = <<t['one']>>, t[two] = <<t['two']>>,
    t[three] = <<t['three']>>, t[four] = <<t['four']>>,
    t[five] = <<t['five']>>, t[six] = <<t['six']>>\n";
    t.forEachAssoc({ key, val: ".. t[<<key>>] = <<val>>\n" });
    "t.getDefaultValue = <<t.getDefaultValue()>>\n";
    "\b";

    t.setDefaultValue(999);
    "t[one] = <<t['one']>>, t[two] = <<t['two']>>,
    t[three] = <<t['three']>>, t[four] = <<t['four']>>,
    t[five] = <<t['five']>>, t[six] = <<t['six']>>\n";
    t.forEachAssoc({ key, val: ".. t[<<key>>] = <<val>>\n" });
    "t.getDefaultValue = <<t.getDefaultValue()>>\n";
    "\b";

    t = ['one' -> 111, 'two' -> 222, 'three' -> 333, 'four' -> 444, *->888];
    "t[one] = <<t['one']>>, t[two] = <<t['two']>>,
    t[three] = <<t['three']>>, t[four] = <<t['four']>>,
    t[five] = <<t['five']>>, t[six] = <<t['six']>>\n";
    t.forEachAssoc({ key, val: ".. t[<<key>>] = <<val>>\n" });
    "t.getDefaultValue = <<t.getDefaultValue()>>\n";
    "\b";

    t = [*->999];
    "t[one] = <<t['one']>>, t[two] = <<t['two']>>,
    t[three] = <<t['three']>>, t[four] = <<t['four']>>,
    t[five] = <<t['five']>>, t[six] = <<t['six']>>\n";
    t.forEachAssoc({ key, val: ".. t[<<key>>] = <<val>>\n" });
    "t.getDefaultValue = <<t.getDefaultValue()>>\n";
    "\b";
}
