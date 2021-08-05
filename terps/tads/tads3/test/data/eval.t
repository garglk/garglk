#include <tads.h>
#include <dynfunc.h>
#include <bignum.h>

obj1: object
    p1 = 'obj1.p1'
    p2 = 'obj1.p2'
;

obj2: object
    p1 = 'obj2.p1'
    p2 = 'obj2.p2'
;

main(args)
{
    local symtab = new LookupTable();
    local macros = new LookupTable();

    symtab['obj1'] = obj1;
    symtab['obj2'] = obj2;
    symtab['p1'] = &p1;
    symtab['p2'] = &p2;
    
    for (;;)
    {
        "eval&gt;";
        local t = inputLine();

        if (t == nil)
            break;

        if (rexMatch('<space>*$', t) != nil)
        {
            "\n";
        }
        else if (rexMatch(
            '<space>*#define<space>+'
            + '(<alpha|_><alphanum|_>*)<space>+(.*)$', t) != nil)
        {
            local name = rexGroup(1)[3];
            local expan = rexGroup(2)[3];
            macros[name] = [expan, [], 0];
            "<<name>> #defined as <<expan>>\b";
        }
        else if (rexMatch(
            '<space>*define<space>+'
            + '(<alpha|_><alphanum|_>*)<space>*'
            + '(<lparen><alphanum|_|space|,>*<rparen>.*)$', t) != nil)
        {
            local name = rexGroup(1)[3];
            t = 'function<<rexGroup(2)[3]>>';
            try
            {
                symtab[name] = new DynamicFunc(t, symtab, nil, macros);
                "<<name>> defined";
            }
            catch (Exception e)
            {
                e.displayException();
            }
            "\b";
        }
        else
        {
            try
            {
                local f = new DynamicFunc(t, symtab, nil, macros);
                local ret = f();
                tadsSay(toString(ret).htmlify());
            }
            catch (Exception e)
            {
                e.displayException();
            }
            "\b";
        }
    }
}
