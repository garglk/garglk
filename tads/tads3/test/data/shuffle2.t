/*
 *   Test of the <<one of>> list class.  This generates some picks for each
 *   named <<one of>> type used in the compiler.  
 */

#include <tads.h>



main(args)
{
    test('purely at random', 'rand');
    test('then purely at random', 'seq,rand');
    test('at random', 'rand-norpt');
    test('then at random', 'seq,rand-norpt');
    test('sticky random', 'rand,stop');
    test('as decreasingly likely outcomes', 'rand-wt');
    test('shuffled', 'shuffle');
    test('then shuffled', 'seq,shuffle');
    test('cycling', 'seq');
    test('stopping', 'seq,stop');
    test('half shuffled', 'shuffle2');
}

test(name, attr)
{
    local lst = new OneOfIndexGen();
    lst.numItems = 10;
    lst.listAttrs = attr;

    "<<name>>: ";
    local trials = 50;
    for (local i in 1..trials)
        "<<lst.getNextIndex()>><<if i < trials>>, ";
    "\b";
}
