/*
 *   Test of address operator '&'.  Starting in 3.1, &x doesn't necessarily
 *   mean that x is a property, as it did in the past.  &x can now be used
 *   for function pointers and built-in function pointers as well.
 *   However, a symbol that's never defined anywhere in the project is
 *   still assumed to be a property as the type of last resort.  This tests
 *   that we can use an unknown symbol with '&' and get a property pointer
 *   by default.
 */

#include <tads.h>

obj1: object
    p1 = 7
;

main(args)
{
    local a = &p1;
    local b = &p2;

    "obj1.(a) = <<obj1.(a)>>, obj1.(b) = <<obj1.(b)>>, dataType(b) = <<
      dataType(b)>>\n";
}
