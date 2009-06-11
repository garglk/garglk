#include "tads.h"
#include "t3.h"

_main(args)
{
    t3SetSay(&_say_embed);
    main();
}

_say_embed(str)
{
    tadsSay(str);
}

main()
{
    test.p3 = 'new property test.p3';
    test.p2 = 'updated test.p2';
}

test: object
    p1 = 'This is test.p1'
    p2 = 'And this is test.p2'
;

