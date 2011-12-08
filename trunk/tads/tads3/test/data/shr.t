#include <tads.h>

main(args)
{
    shr(1, 1);
    shr(128, 1);
    shr(0x7fffffff, 1);
    shr(-1, 1);
    shr(~0, 1);
    shr(-2, 1);
    shr(-3, 1);
    shr(0x80000000, 1);
    shr(0x80000000, 2);
    shr(~0, 16);
}

shr(a, b)
{
    "<<a>> &gt;&gt; <<b>> = <<(a >> b)>>\n";
    "<<a>> &gt;&gt;&gt; <<b>> = <<(a >>> b)>>\b";
}
