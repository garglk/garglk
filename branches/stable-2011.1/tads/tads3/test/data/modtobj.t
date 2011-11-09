#include "tads.h"

modify TadsObject
    test(x) { "TadsObject.test(<<x>>)\n"; }
;

myObject: object
    prop1 = 100
    prop2 = 200
;

globals: PreinitObject
    execute()
    {
        symtab = t3GetGlobalSymbols();
        valToSym = new LookupTable(symtab.getBucketCount(),
                                   symtab.getEntryCount());
        symtab.forEachAssoc({sym, val: valToSym[val] = sym});
    }
    symtab = nil
    valToSym = nil;
;

sayBool(x)
{
    "<<x ? 'yes' : 'no'>>";
}

main(args)
{
    showScList(myObject);

    "myObject defines prop1? <<sayBool(myObject.propDefined(&prop1))>>\n";
    "myObject defines prop2? <<sayBool(myObject.propDefined(&prop2))>>\n";
    "myObject defines execute? <<sayBool(myObject.propDefined(&execute))>>\n";
    "myObject defines propDefined?
        <<sayBool(myObject.propDefined(&propDefined))>>\n";
}

showScList(obj)
{
    local lst = obj.getSuperclassList();

    "object <<globals.valToSym[obj]>>: ";
    for (local i = 1 ; i <= lst.length() ; ++i)
    {
        if (i != 1)
            ", ";
        "<<globals.valToSym[lst[i]]>>";
    }
    "\n";
    for (local i = 1 ; i <= lst.length() ; ++i)
        showScList(lst[i]);
}

