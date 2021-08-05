#include <tads.h>

main(args)
{
    "This is a string with a spurious <<end>>. ";
    "This one has a random <<else>>. ";
    "This one has an <<if box.isOpen>>if, with an extra <<end>><<end>>. ";
    "Here's an unopened switch <<case>>, and a <<default>>. ";
    "Another, with an <<expression error with extra tokens>>, and then
    <<the same thing on the next line>>. ";
}

