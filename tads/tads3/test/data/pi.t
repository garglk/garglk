#include <tads.h>
#include <BigNum.h>

main(args)
{
    local t0 = getTime(GetTimeTicks);
    local x = BigNumber.getPi(512);
    local t1 = getTime(GetTimeTicks);
    "x = <<x.formatString(512)>>\n";
    "delta-t = <<t1 - t0>> millseconds (<<(t1-t0)/1000>> seconds)\n";
}
