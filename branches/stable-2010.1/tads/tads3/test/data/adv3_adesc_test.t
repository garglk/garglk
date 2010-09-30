#include "adv3.h"


object template 'sDescStr';

Thing 'universal remote' articleIndef='a';
Thing 'yttrium block';
Thing 'yellow brick';
Thing 'bowling ball';
Thing 'orange ball';
Thing 'evergreen shrub';
Thing 'äxiom';
Thing 'average score';
Thing 'motorcar';
Thing 'car';

main(args)
{
    forEachInstance(Thing, { cur: "<<cur.aDesc>>\n" });
}

