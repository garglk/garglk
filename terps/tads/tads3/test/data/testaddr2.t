/*
 *   Test of the address operator '&'.  This function tests what happens if
 *   we use &f with a symbol 'f' that's never defined.  The compiler should
 *   presume that the symbol is a property value.  
 */

#include <tads.h>

main(args)
{
    local b = 7; b >>>= 1;
    local a = &f;
    "a = &amp;f: dataType(a) = <<dataType(a)>>, a(7) = <<a(7)>>\n";
}
