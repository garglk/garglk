#include <tads.h>
#include <bignum.h>

main(args)
{
    local lst;

    lst = TadsObject.getPropList();
    foreach (local prop in lst)
        "val = <<TadsObject.propType(prop)>>\n";

    lst = BigNumber.getPropList();
    foreach (local prop in lst)
        "val = <<BigNumber.propType(prop)>>\n";
}

