#include <tads.h>
#include <lookup.h>

myObj: object
    prop1 = 'hello'
    prop2 = 'there'
;

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
    t3RunGC();
    
    "Classes:\n";
    listObjects(ObjClasses);

    "\bObjects:\n";
    listObjects(ObjInstances);

    "\b";
}

listObjects(typeFlags)
{
    for (local obj = firstObj(typeFlags) ; obj != nil ;
         obj = nextObj(obj, typeFlags))
    {
        local name;
        
        name = global.reverseSymtab[obj];
        if (name == nil)
            "unknown\n";
        else
            "<<name>>\n";
    }
}

