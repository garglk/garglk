/*
 *   Address operator '&' test.
 *   
 *   This test checks what happens when we take the address of an external
 *   symbol which happens to be a function.  Prior to 3.1, &x implied that
 *   x was a property name.  Starting in 3.1, &x can also be used to obtain
 *   function pointers and built-in function pointers.  This file tests
 *   that referring to &f doesn't force f to be a property, so that we can
 *   link to a separate file (testaddr1b.t) where f is defined as a function.  
 */

#include <tads.h>

main(args)
{
    local a = &f;
    "a = &amp;f: dataType(a) = <<dataType(a)>>, a(7) = <<a(7)>>\n";
}
