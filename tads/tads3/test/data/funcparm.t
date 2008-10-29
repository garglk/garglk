#include "tads.h"


#define showParams(func) _showParams(#@func, func)
_showParams(funcName, func)
{
    local args;

    /* get the function parameter information */
    args = getFuncParams(func);

    /* show the name and the open paren */
    "<<funcName>>(";

    /* list arguments - name them starting at 'a' (unicode=97) */
    for (local i = 1, local nm = 97, local len = 1 ;
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

varfunc(a, b, ...)
{
    return a + b + argcount;
}

varfunc2(...)
{
    return argcount;
}

main(args)
{
    showParams(main); "\n";
    showParams(varfunc); "\n";
    showParams(varfunc2); "\n";
    showParams(_showParams); "\n";
}
