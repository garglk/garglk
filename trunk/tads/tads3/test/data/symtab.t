#include "tads.h"
#include "t3.h"
#include "lookup.h"

enum RED, GREEN, BLUE;

class Item: object
    sdesc = ""
    ldesc = ""
;

book: Item
    sdesc = "book"
    ldesc = "This is the book."
;

box: Item
    sdesc = "box"
    ldesc = "This is the box."
;

ball: Item
    sdesc = "ball"
    ldesc = "This is the ball."
;

globals: PreinitObject
    execute()
    {
        symtab = t3GetGlobalSymbols();
    }

    /* 
     *   the global symbol table object - stash it here so it remains
     *   around at run-time 
     */
    symtab = nil
;

main(args)
{
    "Symbol table test\n";

    /* check if t3GetGlobalSymbols() gives us anything */
    if (t3GetGlobalSymbols() == nil)
        "No debug symbols available\n";
    else
        "Debug symbols are available\n";

    "book.sdesc: <<sendMessage('book', 'sdesc')>>\n";
    "ball.ldesc: <<sendMessage('ball', 'ldesc')>>\n";
    "sayVal(5): <<callFunc('sayVal', 5)>>\n";
}

sayVal(val)
{
    "This is sayVal: val = <<val>>\n";
}

callFunc(funcName, [args])
{
    local func;

    /* look up the function */
    func = globals.symtab[funcName];
    if (func == nil)
        "no such function: '<<funcName>>'";
    else if (dataType(func) != TypeFuncPtr)
        "not a function: '<<funcName>>'";
    else
    {
        try
        {
            /* invoke the function with the given argument list */
            (func)(args...);
        }
        catch (Exception exc)
        {
            "error calling <<funcName>>(): <<exc.displayException()>>";
        }
    }
}

sendMessage(objName, propName, [args])
{
    local obj, prop;

    /* look up the object */
    obj = globals.symtab[objName];
    if (obj == nil)
        "no such object: '<<objName>>'";
    else if (dataType(obj) != TypeObject)
        "not an object: '<<objName>>'";
    else
    {
        /* look up the property */
        prop = globals.symtab[propName];
        if (prop == nil)
            "no such property: '<<propName>>'";
        else if (dataType(prop) != TypeProp)
            "not a property: '<<propName>>'";
        else
        {
            try
            {
                /* invoke the property with the given argument list */
                obj.(prop)(args...);
            }
            catch (Exception exc)
            {
                "error calling <<objName>>.<<propName>>:
                <<exc.displayException()>>";
            }
        }
    }
}

