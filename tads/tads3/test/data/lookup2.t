#include <tads.h>

main(args)
{
    local t;

    t = ['one' -> 1, 'two' -> 2, 'three', 'four'->4, 'five'->5];
    t = ['one', 'two'->2, 'three'->3, 'four'->4];
    t = ['one' -> 1, 'two', 'three', 'four'];
    t = ['one' -> 111, 'two' -> 222, 'three' -> 333, *->999, 'four' -> 444];
    t = [*->999, 'one' -> 111, 'two' -> 222, 'three' -> 333, 'four' -> 444];
}
