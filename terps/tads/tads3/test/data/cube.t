#include "tads.h"

main(args)
{
  local c1 = cube(1), c2 = cube(2), c3 = cube(3);
  local cc3 = cube(c3);

  "1 cubed = <<c1>>, 2 cubed = <<c2>>, 3 cubed = <<c3>>\n";
  "3 cubed cubed <<cc3>>\n";
}

cube(n)
{
  return n * n * n;
}
