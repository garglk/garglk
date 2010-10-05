#include "tads.h"

main(args)
{
    "<<func(args)>>\n";
}

func(args)
{
    switch(args.length())
    {
    case 1:
        return 'one';

    case 2:
        return 'two';

    case 3:
        return 'three';
    }

    return 'four or more';
}
