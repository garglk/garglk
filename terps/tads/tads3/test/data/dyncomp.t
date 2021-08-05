#include <tads.h>
#include <dynfunc.h>
#include <bignum.h>
#include <strbuf.h>

class A: object
    p1 = 1
    name = 'A'
    m1(x) { return 'greetings from <<name>>.m1(<<x>>)!'; }
;

test1: A
    name = 'test1'
;

test2: A
    name = 'test2'
;

#define test(src, args) _test(src, nil, 'f' ## #@args, {f: f args})
#define test2(src, call) _test(src, nil, #@call, {f: call})
#define test3(src, call) _test(src, nil, #@call, new function(f) { call })

#define RTEST(a, b) 'This is RTEST(<<a>>, <<b>>)!!!'

class SavedFunc: object
    construct(f, desc, caller)
    {
        f_ = f;
        desc_ = desc;
        caller_ = caller;

        if ((src_ = f.getSource()) != nil)
            src_ = src_.htmlify();
    }

    invoke()
    {
        try
        {
            "Source: <<src_>>\n
            Call: <<desc_>>\n
            Result: <<(self.caller_)(f_)>>";
        }
        catch (Exception exc)
        {
            "Execution error: <<exc.displayException>>";
        }
        
        "\b";
    }

    f_ = nil
    desc_ = nil
    caller_ = nil
    src_ = nil
;

savedFuncs: object
    savelist = []
;

modify List
    myjoin()
    {
        local s = new StringBuffer();
        for (local i = 1 ; i <= length() ; ++i)
        {
            if (i > 1)
                s.append(', ');
            s.append(self[i]);
        }
        return toString(s);
    }
;

main(args)
{
    if (args.length() != 3 || args[2] not in ('-save', '-restore'))
    {
        "usage: dyncomp -save|-restore file\n";
        return;
    }

    if (args[2] == '-restore')
    {
        "Restoring...\n";
        restoreGame(args[3]);
        foreach (local sf in savedFuncs.savelist)
            sf.invoke();

        return;
    }

    test('function(x) { return x*2; }', (7));
    test('function(x, x, x) { return x*x; }', (7));
    test('function(x, p, y) { return x.(p)(y + 1000); }', (test1, &m1, 456));
    test('function(x, y) { return x.m1(y + 1000); }', (test1, 678));
    test('function(x, y) { return x.mUndef(y + 1000); }', (test1, 678));
    test('test1.m1(123);', ());
    test('test2.m1(123);', ());
    test('function(x) { return test2.m1(x + 1000); }', (789));
    test('function(x) { return f1(x + 1000); }', (654));
    test('new BigNumber(\'123.456\');', ());
    test2('function(x) { local a = x*2, '
          + '''f = {x, y: 'anon f! x=<\<x>>, y=<\<y>>, a=<\<a>>'}; '''
          + 'a += 1; return f; }',
          f(135)(7, 8));
    test2('function(x, y) { '
          + 'local f = {: ++x, ++y, \'this is anon 1: x=<\<x>>, y=<\<y>>\' }; '
          + 'local g = {: ++x, ++y, \'this is anon 2: x=<\<x>>, y=<\<y>>\' }; '
          + 'return [f, g]; }', testAnon(f));
    test('function(a:, b:) { return a+b*1000; }', (b:123, a:789));
    test('''function() { local lst = [10, 20, 30, 40, 50]; local sum = 0;
          for (local i in lst) sum += i; return sum; }''', ());
    test('''function() { nameTest(b:123, a:'hello'); }''', ());
    test('''function() { local obj = new Thing('test'); obj.desc; }''', ());
    test('''function(cnt) {
        for (local i in 1..cnt)
          "[\<<i>>] Embedded item number \<<one of>>one\<<or>>two\<<or
          >>three\<<or>>four\<<cycling>>... ";
        }''', (10));
    test('''function([lst]) {
        for (local x in lst)
          "Embedded if test: x=\<<x>>, which is \<<
          if x > 5>>greater than\<<else if x < 5>>less than\<<else
          >>exactly\<<end>> five... ";
        }''', (7, 3, 5, 9, 4));

    Compiler.defineFunc('square', 'function(x) { return x*x; }');
    test('function(x) { return square(x); }', (11));

    test('function(x, y) { return RTEST(x+1, y+1); }', (101, 202));

    localtest('function() { return i; }', 'this is the \'i\' value!');
    localtest('function() { return i; }', 'another \'i\' value!');
    localtest2();
    localtest3();
    localtest4();

    saveGame(args[3]);
}

class Thing: object
    construct(name) { self.name = name; }
    name = 'Thing'
    desc = "This is a Thing whose name is '<<name>>'!\n"
;

f1(x)
{
    return 'Greetings from f1(<<x>>)!';
}

localtest(src, i)
{
    _test(src, t3GetStackTrace(1, T3GetStackDesc).frameDesc_,
          'localtest()', {f: f()});
}

localtest2()
{
    local i = 'original \'i\' value';

    _test('function() { local orig = i; i += \' *MODIFIED!*\'; '
          + 'return orig; }',
          t3GetStackTrace(1, T3GetStackDesc).frameDesc_,
          'localtest2_part1()', {f: f()});
    _test('function() { return i; }',
          t3GetStackTrace(1, T3GetStackDesc).frameDesc_,
          'localtest2_part2()', {f: f()});

    return i;
}

localtest3()
{
    local i = 'first i value';
    local f = {: i += ' [modified by anon fn]' };

    _test('function() { local orig = i; i += \' [modified by dynfunc]\'; '
          + 'return orig; }',
          t3GetStackTrace(1, T3GetStackDesc).frameDesc_,
          'localtest3_part1()', {f: f()});

    f();

    _test('function() { return i; }',
          t3GetStackTrace(1, T3GetStackDesc).frameDesc_,
          'localtest3_part2()', {f: f()});

    return i;
}

localtest4()
{
    local cnt = 0;
    local frame = t3GetStackTrace(1, T3GetStackDesc).frameDesc_;
    local f = new DynamicFunc('function(val) { if (val > 3) ++cnt; }',
                              nil, frame);

    "\blocaltest4:\n";
    local tf = function(n) {
        ". test(<<n>>): before cnt=<<cnt>>, ";
        f(n);
        "after cnt=<<cnt>>\n";
    };

    tf(1);
    tf(2);
    tf(3);
    tf(4);
    tf(5);
}

nameTest(a:, b:)
{
    "Named argument test: a=<<a>>, b=<<b>>\n";
}

testAnon(f)
{
    local l = f(123, 456);
    "l = f(123, 456):\n
    type(l)=<<dataType(l)>>, l.length=<<l.length()>>\n
    type(l[1])=<<dataType(l[1])>>, type(l[2])=<<dataType(l[2])>>\n
    l[1]()=<<l[1]()>>, l[2]()=<<l[2]()>>\n";

    for (local i = 1 ; i <= l.length() ; ++i)
        savedFuncs.savelist += new SavedFunc(l[i], 'l[<<i>>]()', {lf: lf()});
}

_test(src, locals, desc, call)
{
    try
    {
        local f = Compiler.compile(src, locals);
        local sf = new SavedFunc(f, desc, call);

        savedFuncs.savelist += sf;
        sf.invoke();
    }
    catch (Exception exc)
    {
        "Source: <<src.htmlify()>>\n
        Call: <<desc.htmlify()>>\n
        Compiler error\n<<exc.displayException>>\b";
    }
}
