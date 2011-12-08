#include <tads.h>

main(args)
{
    for (local i in 1..50)
    {
        "Nested if &gt; oneof [<<i>>]: <<if i < 25>><<
          one of>>red<<or>>blue<<or>>green<<at random>><<else
          >><<one of>>yellow<<or>>orange<<or>>purple<<at random>><<
          end>>\n";
    }
}
