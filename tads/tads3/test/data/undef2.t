#include <tads.h>
#include <lookup.h>

property propNotDefined;
export propNotDefined;

object template "name";

symtab: PreinitObject
    execute()
    {
        local gsym;

        /* get the global symbol table */
        gsym = t3GetGlobalSymbols();

        /* create a table for mapping property ID's to names */
        proptab_ = new LookupTable(
            gsym.getBucketCount(), gsym.getEntryCount());

        /* build the table */
        gsym.forEachAssoc(new function(key, val)
        {
            /* 
             *   if it's a property, add it to the property lookup table
             *   (the property ID is the key, and the name is the value,
             *   so it provides the reverse of the system global symbol
             *   table's mapping) 
             */
            if (dataType(val) == TypeProp)
                proptab_[val] = key;
        });
    }
    proptab_ = nil

    getPropName(prop)
    {
        local name;

        /* look up the name in our property ID mapping table */
        name = proptab_[prop];

        /* if it's defined, return it; otherwise, provide a default name */
        return (name != nil ? name : '<undefined>');
    }
;

modify Object
    propNotDefined(prop, [args])
    {
        local first;
        
        "Undefined: <<name>>.<<symtab.getPropName(prop)>>(";
        first = true;
        foreach(local x in args)
        {
            /* 
             *   show a comma after the previous arg, if there was a
             *   previous arg 
             */
            if (!first)
            {
                /* it's not the first, so there's a previous */
                ", ";
            }

            /* show this one */
            "<<x>>";

            /* the next one won't be the first, so clear the flag */
            first = nil;
        }
        ")\n";
    }
;

class Item: object
;

obj1: Item "obj1"
    a(x) { "This is obj1.a(<<x>>)\n"; }
    b(x, y) { "This is obj1.b(<<x>>, <<y>>)\n"; }
;


obj2: Item "obj2"
    a(x) { "This is obj2.a(<<x>>)\n"; }
;

main(args)
{
    obj1.a(1);
    obj1.b(2, 3);
    obj2.a(4);
    obj2.b(5, 6);
}

