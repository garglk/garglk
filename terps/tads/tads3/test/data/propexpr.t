#include <tads.h>

foo: object
    setVal(val) { self.val = val; }
    bad = (val)
    good() { return val; }
    val = nil
;

main(args)
{
    foo.setVal('testing');
    "dataType(foo.bad) = <<dataType(foo.bad)>>\n";
    "foo.bad = <<foo.bad>>\n";
}
