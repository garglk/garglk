#include "tads.h"
#include "t3.h"
#include "vector.h"

main(args)
{
    local vec;

    if (args.length() > 1 && args[2] == 'restore')
    {
        "Restoring...\n";
        restoreGame('iter.t3s');
        "Restored!\n";
        goto afterRestore;
    }

    args = ['hello', 'there', 'how', 'are', 'you'];
    args += 'today';

    iterate('Constant list:', [1, 2, 3, 4, 5]);
    iterate('List:', args);
    iterate('Vector:', vec = new Vector(10, args));

    "Vector with fill in progress:\n";
    for (local iter = vec.createIterator() ; iter.isNextAvailable() ; )
    {
        vec.applyAll({x: x + '!'});
        local x = iter.getNext();
        "Next is <<x>>\n";
    }
    "\b";

    "Saving after third element:\n";
    for (local iter = vec.createIterator(), local i = 1 ;
         iter.isNextAvailable() ; ++i)
    {
        local x = iter.getNext();
        "Next is <<x>>\n";
        if (i == 3)
        {
            globals.vec_ = vec;
            globals.iter_ = iter;
            globals.i_ = i;
            saveGame('iter.t3s');

        afterRestore:
            vec = globals.vec_;
            iter = globals.iter_;
            i = globals.i_;
        }
    }
    "\b";

    "Current position check:\n";
    for (local iter = vec.createIterator() ; iter.isNextAvailable() ; )
    {
        iter.getNext();
        "Current index is <<iter.getCurKey()>>, value is
        <<iter.getCurVal()>>\n";
    }

    "\bReset check:\n";
    for (local passno = 1, local iter = vec.createIterator() ;
         iter.isNextAvailable() ; )
    {
        iter.getNext();
        "Current index is <<iter.getCurKey()>>, value is
        <<iter.getCurVal()>>\n";

        if (!iter.isNextAvailable() && passno == 1)
        {
            ++passno;
            iter.resetIterator();
        }
    }
}

globals: object
    vec_ = nil
    iter_ = nil
    i_ = nil
;

iterate(hdr, obj)
{
    "<<hdr>>\n";
    
    for (local iter = obj.createIterator() ; iter.isNextAvailable() ; )
    {
        local x = iter.getNext();
        "Next is <<x>>\n";
    }

    "\b";
}

