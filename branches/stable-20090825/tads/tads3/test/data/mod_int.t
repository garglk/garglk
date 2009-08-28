#include <tads.h>
#include <lookup.h>

myglob: PreinitObject
    execute()
    {
        symtab = t3GetGlobalSymbols();
        revtab = new LookupTable();
        symtab.forEachAssoc({key,val: revtab[val] = key});
    }
    symtab = nil
    revtab = nil
;

modify TadsObject
    m1 { "This is method 1.\n"; }
;

modify TadsObject
    m2 { "This is method 2.\n"; }
;

myObj: object
;

main(args)
{
    myObj.m1;
    myObj.m2;
    foreach (local cur in TadsObject.getPropList())
        "<<myglob.revtab[cur]>>\n";
}

