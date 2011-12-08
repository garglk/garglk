#include <tads.h>
#include <strbuf.h>
#include <reflect.h>

modify List
    myjoin()
    {
        local s = new StringBuffer();
        for (local i = 1, local len = length() ; i <= len ; ++i)
        {
            if (i > 1)
                s.append(', ');
            s.append(self[i]);
        }
        return toString(s);
    }
;

g(b)
{
    f(b);
}

f(a:, b)
{
    "This is func! a=[<<a>>], b=[<<b>>] ";
    "(all named arguments: <<t3GetNamedArgList().mapAll(
        { a: a + '=' + t3GetNamedArg(a, nil) }).myjoin()>>)\n";
}

o: object
    m(x) { g(x); }
;

o2: o
    m(a:, x) { inherited(a: 'inherited ' + a, x); }
;

h(a:)
{
    "This is h! a=<<a>>\n";
    return new function(a:)
    {
        "This is anonymous! a=<<a>> ";
        "(all named arguments: <<t3GetNamedArgList().mapAll(
            { a: a + '=' + t3GetNamedArg(a, nil) }).myjoin()>>)\n";

        local t = t3GetStackTrace(nil, T3GetStackLocals);
        "Stack trace:\n";
        foreach (local f in t)
        {
            f.func_;
            local fn = [j->'j', k->'k', h->'h', main->'main', _main->'_main',
                        *->'{anonymous}'][f.func_];
            if (f.obj_ != nil && f.obj_.ofKind(AnonFuncPtr))
                fn = '{anonymous}';
            "<<fn>>(";
            if (f.namedArgs_ != nil)
                "<<f.namedArgs_.keysToList().mapAll(
                    {key: key + ':' + f.namedArgs_[key]}).myjoin()>>";
            ")\n";
        }
    };
//    return ({ a: : "This is anonymous! a=<<a>>\n" });
}

j(func)
{
    func();
}

k(func)
{
    j(func, c: 'C from k()');
}

def1(a: = 10, b:?)
{
    "This is def1: a=<<a>>, b=<<b>>\n";
}

def2(a := 10, b := a*10, c := b*100)
{
    "This is def2: a=<<a>>, b=<<b>>, c=<<c>>\n";
}

class Thing: object
;
class Container: Thing
;

mm(Thing t, a:, Thing o)
{
    "This is mm(Thing): a=<<a>>\n";
}

mm(Container c, a:, Thing o)
{
    "This is mm(Container): a=<<a>>\n";
}

main(args)
{
    try { f('B 1'); } catch (Exception e) { "<<e.displayException>>\n"; }
    f(a: 'A 2', 'B 2');
    try { g('B 3'); } catch (Exception e) { "<<e.displayException>>\n"; }
    g(a: 'A 4', 'B 4', c: 'C 4');

    local l = ['B 5'];
    g(a: 'A 5', l...);

    o.m(a: 'A 6', 'B 6');
    o2.m(a: 'A 7', 'B 7');

    local x = h(a: 'A 8');
    j(x, a: 'A 9', b: 'B 9', c:'C 9');
    k(x, a: 'A 10', b: 'B 10', c: 'C 10');

    local t = new Thing();
    mm(t, t, a:'thing');

    t = new Container();
    mm(t, t, a:'container');

    [1, 2, 3].forEach(a: 'A 11',
                      { x, a: : "List.forEach: x=<<x>>, a=<<a>>\n" });

    def1();
    def1(a: 5);
    def1(b: 7);
    def1(a: 9, b:11);
    def2();
    def2(a: 9);
    def2(b: 99);
    def2(c: 999);
    def2(a: 8, b: 9);
    def2(a: 11, b: 12, c: 13);
    def2(a: 14, c: 15);
    def2(b: 16, c: 17);
}
