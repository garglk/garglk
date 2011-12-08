#include <tads.h>
#include <dynfunc.h>

main(args)
{
    local a = 'A!';
    local b = 'B!';

    print('a = {a}, b = {b}\n');

    local fr = t3GetStackTrace(1, T3GetStackDesc).frameDesc_;
    Compiler.eval('a += \'*MOD*\'', fr);
    "After eval: a=<<a>>, b=<<b>>\n";

    f();
}

f()
{
    local b = 'f.B!';
    useVal(b);

    local fr = t3GetStackTrace(nil, T3GetStackDesc).mapAll({x: x.frameDesc_});
    tadsSay(Compiler.eval('\'a=<\<a>\>, b=<\<b>\>\\n\'', fr));
}

useVal(x)
{
}

print(msg)
{
    local f = t3GetStackTrace(2, T3GetStackDesc).frameDesc_;
    tadsSay(rexReplace('<lbrace>(<^rbrace>*)<rbrace>', msg,
                       {m: Compiler.eval(rexGroup(1)[3], f)}));
}
