#include "tads.h"
#include "lookup.h"

myObj: object
    prop1 = 'hello'
    prop2 = 789
    prop3 = true
    prop4 = [1, 2, 3]
;

otherObj: object
    prop1 = 'goodbye'
    prop7 = [9, 8, 7, 6, 5]
    method1(x, y) { return x * y; }
    method2(a, b, c) { return a + b + c; }
    method3(a, ...) { return a + argcount; }
;

globals: PreinitObject
    execute()
    {
        local symtab;
        
        /* get the global symbol table */
        symtab = t3GetGlobalSymbols();

        /* create our value-to-symbol tables */
        propToSym = new LookupTable(128, 256);
        objToSym = new LookupTable(128, 256);

        /* build our reverse tables */
        symtab.forEachAssoc(new function(sym, val)
        {
            switch(dataType(val))
            {
            case TypeObject:
                /* add it to the object name table */
                objToSym[val] = sym;
                break;

            case TypeProp:
                /* add it to the property name table */
                propToSym[val] = sym;
                break;
            }
        });
    }
    propToSym = nil
    objToSym = nil
    namelist = []
;

main(args)
{
    showProps(myObj);
    showProps(otherObj);
}

showProps(obj)
{
    /* show the object name */
    "object <<globals.objToSym[obj]>>:\n";

    /* show the properties of this object */
    showObjProps(obj, obj, 1);
}

showObjProps(selfObj, obj, level)
{
    local lst;

    /* get this object's properties */
    lst = obj.getPropList();

    /* show each */
    foreach (local prop in lst)
    {
        local args;

        /* indent */
        indent(level);

        /* show the property name */
        "<<globals.propToSym[prop]>>";

        /* show the arg list, if there is one */
        args = selfObj.getPropParams(prop);
        if (args[1] != 0 || args[2] != 0 || args[3])
        {
            local len;

            /* start with the open paren */
            "(";

            /* list arguments - name them starting at 'a' (unicode=97) */
            for (local i = 1, local nm = 97, len = 1 ;
                 i <= args[1] + args[2] ; ++i, ++nm)
            {
                /* add a comma separator if this isn't the first one */
                if (i != 1)
                    ", ";

                /* show this one's name (a, b, ...) */
                "<<makeString(nm, len)>>";

                /* add '?' if this one's optional (past the minimum count) */
                if (i > args[1])
                    "?";

                /* 
                 *   if we've reached 'z' (unicode=122), go back to 'a'
                 *   and increase the length, so we get x, y, z, aa, bb,
                 *   cc, ..., zz, aaa, bbb, ... 
                 */
                if (nm == 122)
                {
                    /* go back to a minus one (we're about to increment) */
                    nm = 96;

                    /* increase the length */
                    ++len;
                }
            }

            /* if it's varargs, add the varargs signifier */
            if (args[3])
            {
                /* show a comma if there were other arguments */
                if (args[1] != 0 || args[2] != 0)
                    ", ";

                /* show the varargs ellipsis */
                "...";
            }

            /* add the closing paren */
            ")";
        }
        "\n";
    }

    /* go through our superclass list */
    foreach (local sc in obj.getSuperclassList())
    {
        /* indent */
        indent(level);
        
        /* show the header */
        "inherited from <<globals.objToSym[sc]>>:\n";

        /* show the list */
        showObjProps(selfObj, sc, level + 1);
    }
}

indent(level)
{
    for (local i = 0 ; i < level ; ++i)
        "\ \ ";
}
