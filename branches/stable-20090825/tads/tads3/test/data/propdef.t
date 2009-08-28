#include <tads.h>

#include <lookup.h>

global: PreinitObject
    execute()
    {
        symtab = t3GetGlobalSymbols();
        reverseSymtab = new LookupTable();
        symtab.forEachAssoc({key, val: global.reverseSymtab[val] = key});
    }
    symtab = nil
    reverseSymtab = nil
;

main(args)
{
    local obj = TadsObject.propDefined(&propDefined, PropDefGetClass);
    if (obj == nil)
        "undefined\n";
    else
    {
        local str = global.reverseSymtab[obj];
        if (str == nil)
            "unnamed\n";
        else
            "<<str>>\n";
    }
}
