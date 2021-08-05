#charset "latin-1"

/*
 *   built-in function tests 
 */

#include <t3.h>
#include <tads.h>
#include <bignum.h>

obj1: object
    prop1 = "Hello"
;

_say_embed(str) { tadsSay(str); }


_main(args)
{
    try
    {
        main();
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

function main()
{
    t3SetSay(_say_embed);
    tadsSay('built-in function tests\n');

    local str = 'abcdefghijklmnopqrstuvwxyz';
    tadsSay('str.length() = ' + str.length() + '\n');
    tadsSay('str.substr(5) = [' + str.substr(5) + ']\n');
    tadsSay('str.substr(7, 11) = [' + str.substr(7, 11) + ']\n');
    tadsSay('str.substr(7, 30) = [' + str.substr(7, 30) + ']\n');
    tadsSay('str.substr(30) = [' + str.substr(30) + ']\n');
    tadsSay('str.substr(30, 8) = [' + str.substr(30, 8) + ']\n');

    tadsSay('\b');
    tadsSay('str.startsWith(a) = '
            + (str.startsWith('a') ? 'yes' : 'no') + '\n');
    tadsSay('str.startsWith(b) = '
            + (str.startsWith('b') ? 'yes' : 'no') + '\n');
    tadsSay('str.startsWith(abc) = '
            + (str.startsWith('abc') ? 'yes' : 'no') + '\n');
    tadsSay('str.startsWith(abcdef) = '
            + (str.startsWith('abcdef') ? 'yes' : 'no') + '\n');
    tadsSay('str.startsWith(bc) = '
            + (str.startsWith('bc') ? 'yes' : 'no') + '\n');
    tadsSay('str.startsWith(abcefg) = '
            + (str.startsWith('abcefg') ? 'yes' : 'no') + '\n');

    tadsSay('\b');
    tadsSay('str.endWith(z) = '
            + (str.endsWith('z') ? 'yes' : 'no') + '\n');
    tadsSay('str.endWith(y) = '
            + (str.endsWith('y') ? 'yes' : 'no') + '\n');
    tadsSay('str.endsWith(xyz) = '
            + (str.endsWith('xyz') ? 'yes' : 'no') + '\n');
    tadsSay('str.endsWith(tuvwxyz) = '
            + (str.endsWith('tuvwxyz') ? 'yes' : 'no') + '\n');
    tadsSay('str.endsWith(xy) = '
            + (str.endsWith('xy') ? 'yes' : 'no') + '\n');
    tadsSay('str.endsWith(tuvxyz) = '
            + (str.endsWith('tuvxyz') ? 'yes' : 'no') + '\n');

    tadsSay('\b');
    local lst = ['one', 'two', 'three', 'four', 'five',
                 'six', 'seven', 'eight', 'nine', 'ten'];
    tadsSay('lst.length() = ' + lst.length() + '\n');

    tadsSay('lst.sublist(5) = '); sayList(lst.sublist(5)); tadsSay('\n');
    tadsSay('lst.sublist(5, 3) = '); sayList(lst.sublist(5, 3)); tadsSay('\n');
    tadsSay('lst.sublist(5, 15) = '); sayList(lst.sublist(5, 15)); tadsSay('\n');
    tadsSay('lst.sublist(15) = '); sayList(lst.sublist(15)); tadsSay('\n');
    tadsSay('lst.sublist(15, 3) = '); sayList(lst.sublist(15, 3)); tadsSay('\n');

    tadsSay('lst.car() = '); tadsSay(lst.car()); tadsSay('\n');
    tadsSay('lst.cdr() = '); sayList(lst.cdr()); tadsSay('\n');
    tadsSay('[].car() = nil? ' + ([].car() == nil ? 'yes' : 'no') + '\n');
    tadsSay('[].cdr() = nil? ' + ([].cdr() == nil ? 'yes' : 'no') + '\n');

    "lst - 'three' -> <<sayList(lst - 'three')>>\n";
    "lst - ['four', 'eight'] -> <<sayList(lst - ['four', 'eight'])>>\n";
    "lst - ['four', '123', 'nine'] ->
        <<sayList(lst - ['four', '123', 'nine'])>>\n";

    local lst2 = ['one', [2, 3], 'four'];
    "lst2 == ['one', [2, 3], 'four']:
         <<bool2str(lst2 == ['one', [2, 3], 'four'])>>\n";
    "lst2 == ['one', 2, 3, 'four']:
         <<bool2str(lst2 == ['one', 2, 3, 'four'])>>\n";
    "lst2 == ['one', [3, 4], 'four']:
         <<bool2str(lst2 == ['one', [3, 4], 'four'])>>\n";

    "\bdataType() tests\n";
    lst[1] = 'ONE';
    str += '1234567890';
    "dataType(nil) = <<dataType(nil)>>\n";
    "dataType(true) = <<dataType(true)>>\n";
    "dataType(obj1) = <<dataType(obj1)>>\n";
    "dataType(&prop1) = <<dataType(&prop1)>>\n";
    "dataType(123) = <<dataType(123)>>\n";
    "dataType('hello') = <<dataType('hello')>>\n";
    "dataType('<<str>>' /*dynamic string*/) = <<dataType(str)>>\n";
    "dataType([1, 2, 3]) = <<dataType([1, 2, 3])>>\n";
    "dataType(<<sayList(lst)>> /*dynamic list*/) = <<dataType(lst)>>\n";
    "dataType(_main) = <<dataType(_main)>>\n";

    "\bvarargs tests\n";
    "varfunc(1): << varfunc(1) >>\n";
    "varfunc(1, 2, 3): << varfunc(1, 2, 3) >>\n";

    local str2 = 'This is a TEST of UPPER and lower. 123;!@#$';
    "\bupper/lower test\n";
    "upper('<<str2>>') = '<<str2.toUpper()>>'\n";
    "lower('<<str2>>') = '<<str2.toLower()>>'\n";
    "title('<<str2>>') = '<<str2.toTitleCase()>>'\n";
    "caseFold('<<str2>>') = '<<str2.toFoldedCase()>>'\n";

    local str3 = 'We\u0130ﬂstr‰ﬂe O\ufb03cepl‰tz';
    "upper('<<latin1ify(str3)>>') = '<<latin1ify(str3.toUpper())>>'\n";
    "lower('<<latin1ify(str3)>>') = '<<latin1ify(str3.toLower())>>'\n";
    "title('<<latin1ify(str3)>>') = '<<latin1ify(str3.toTitleCase())>>'\n";
    "caseFold('<<latin1ify(str3)>>') = '<<latin1ify(str3.toFoldedCase())>>'\n";

    "\btoString tests\n";
    "toString(123) = <<toString(123)>>\n";
    "toString(123, 8) = <<toString(123, 8)>>\n";
    "toString(123, 16) = <<toString(123, 16)>>\n";
    "toString(true) = <<toString(true)>>\n";
    "toString(nil) = <<toString(nil)>>\n";
    "toString(0xffffffff, 10) = <<toString(0xffffffff, 10)>>\n";
    "toString(0xffffffff, 16) = <<toString(0xffffffff, 16)>>\n";
    "toString(0xffffffff, 8) = <<toString(0xffffffff, 8)>>\n";
    "toString(0xffffffff, 2) = <<toString(0xffffffff, 2)>>\n";
    "toString(0xffffffff, 10, nil) = <<toString(0xffffffff, 10, nil)>>\n";
    "toString(0xffffffff, 16, true) = <<toString(0xffffffff, 16, true)>>\n";
    "toString(0xffffffff, 8, true) = <<toString(0xffffffff, 8, true)>>\n";
    "toString(0xffffffff, 2, true) = <<toString(0xffffffff, 2, true)>>\n";
    "toString([1,2,3]) = <<toString([1, 2, 3])>>\n";
    "toString([true, 'x', nil, 7]) = <<toString([true, 'x', nil, 7])>>\n";
    "toString(Vector.generate({i: i*10},3) = <<
      toString(Vector.generate({i: i*10}, 3))>>\n";
    "toString([100, 1000, 10000], 16) = <<
      toString([100, 1000, 10000], 16)>>\n";
    "toString([100, [200, 300], 400], 8) = <<
      toString([100, [200, 300], 400], 8)>>\n";

    "\btoInteger tests\n";
    "toInteger('nil') = <<bool2str(toInteger('nil'))>>\n";
    "toInteger('true') = <<bool2str(toInteger('true'))>>\n";
    "toInteger('  nil   ') = <<bool2str(toInteger(' nil   '))>>\n";
    "toInteger(' true   ') = <<bool2str(toInteger(' true   '))>>\n";
    testToInt(' + 123');
    testToInt('ffff', 16);
    testToInt('123', 8);
    testToInt('11111111', 2);
    testToInt('100', 12);
    testToInt('ABCDEF', 16);
    testToInt('abcdef', 15);
    testToInt('ABCDEF', 14);
    testToInt(' - ABCDEF', 13);
    testToInt('ABCDEF', 12);
    testToInt('ABCDEF', 11);
    testToInt('ABCDEF', 10);
    testToInt('6789', 10);
    testToInt('6789', 9);
    testToInt('6789', 8);
    testToInt('6789', 7);
    testToInt('2147483640');
    testToInt('2147483647');
    testToInt('2147483648');
    testToInt('2147483649');
    testToInt('2147483650');
    testToInt('-2147483647');
    testToInt('-2147483648');
    testToInt('-2147483649');
    testToInt('7fffffff', 16);
    testToInt('80000000', 16);
    testToInt('80000001', 16);
    testToInt('-7fffffff', 16);
    testToInt('-80000000', 16);
    testToInt('-80000001', 16);
    testToInt('ffffffff', 16);
    testToInt('ffffffff1', 16);
    testToInt('11fffffff', 16);
    testToInt('1ffffffff', 16);

    testToNum('ABCDEF', 16);
    testToNum('7fffffff', 16);
    testToNum('80000000', 16);
    testToNum('100000000', 16);
    testToNum('ffffffffffffffff', 16);
    testToNum('ZZZZZZZ', 36);
    testToNum('1000000000');
    testToNum('2000000000');
    testToNum('4000000000');
    testToNum('1.234');
    testToNum('1234e');
    testToNum('1234e+');
    testToNum('1234e+20e');
    testToNum('.1234e+2');
    testToNum('1234.');
    testToNum(true);
    testToNum(nil);
    testToNum('  true  ', 16);
    testToNum('  nil  ', 7);

    "\b";
    testAbs(17);
    testAbs(-17);
    testAbs(0);
    testAbs(3.1415);
    testAbs(sum(3.1415, 17));
    testAbs(sum(3.1415, -17));
    testAbs(sum(3.1415, -3.1415));

    "\b";
    testSgn(17);
    testSgn(-17);
    testSgn(0);
    testSgn(3.1415);
    testSgn(sum(3.1415, 17));
    testSgn(sum(3.1415, -17));
    testSgn(sum(3.1415, -3.1415));

    "\b";
    testConcat();
    testConcat('one');
    testConcat('one', 'two');
    testConcat('one', 'two', 'three');
    testConcat(111);
    testConcat(111, 222);
    testConcat(111, 222, 333);
    testConcat(111, 'two', 333);
    testConcat(111, 222.222, 'three');

#if 0
    // omit these from the automated tests - the times vary, so we can't
    // mechanically compare the results (we need some kind of fixed-time
    // mode that always returns the same value to test getTime mechanically)
    //
    "getTime() test\n";
    "getTime() = <<sayList(getTime())>>\n";
    "getTime(1) = <<sayList(getTime(1))>>\n";
    "getTime(2) = <<getTime(2)>>\n";
#endif

    "\bfind() tests\n";
    "'<<str>>'.find('') = <<str.find('')>>\n";
    "'<<str>>'.find('abcdef') = <<str.find('abcdef')>>\n";
    "'<<str>>'.find('xyz') = <<str.find('xyz')>>\n";
    "'<<str>>'.find('1234567890') = <<str.find('1234567890')>>\n";
    "'<<str>>'.find('xxx') = <<str.find('xxx')>>\n";
    testFind(str, R'%d+');
    for (local i = 1 ; i <= 5 ; ++i)
        "'<<str>>'.find('cde', <<i>>) = <<str.find('cde', i)>>\n";

    "\bfindLast() tests\n";
    testFindLast(str, '');
    testFindLast(str, 'abcdef');
    testFindLast(str, 'xyz');
    testFindLast(str, '1234567890');
    testFindLast(str, 'xxx');
    testFindLast(str, R'%d+');
    testFindLast(str, R'<fe>%d+');
    testFindLast(str, R'<min>%d+');
    testFindLast('aaaaaaaabaaa', R'<fe>a+ba+|b+');
    testFindLast(str, R'b.*9|%d+8');
    testFindLast(str, R'<fe>b.*9|%d+8');
    for (local i in 7..1 step -1)
        testFindLast(str, 'cde', i);

    testFindLast('abc ade afg ahi', 'a');
    for (local i in 0..-16 step -1)
        testFindLast('abc ade afg ahi', 'a', i);

    "\bfind()-with-index tests\n";
    str = 'abcdefabcghiabcjklabcmnoabcpqrabcstuabcvwxabcyz...f';
    for (local i in 1..10)
        "'<<str>>'.find('abc', <<i>>) = <<str.find('abc', i)>>\n";
    for (local i in -1..-10 step -1)
        "'<<str>>'.find('abc', <<i>>) = <<str.find('abc', i)>>\n";

    local pat = new RexPattern('a.*f');
    for (local i in 1..10)
        "'<<str>>'.find(re 'a.*f', <<i>>) = <<str.find(pat, i)>>\n";
    for (local i in -1..-10 step -1)
        "'<<str>>'.find(re 'a.*f', <<i>>) = <<str.find(pat, i)>>\n";

    "\bindexOf() tests\n";
    "<<sayList(lst)>>.indexOf('one') = <<lst.indexOf('one')>>\n";
    "<<sayList(lst)>>.indexOf('ONE') = <<lst.indexOf('ONE')>>\n";
    "<<sayList(lst)>>.indexOf('two') = <<lst.indexOf('two')>>\n";
    "<<sayList(lst)>>.indexOf('ten') = <<lst.indexOf('ten')>>\n";
    "<<sayList(lst)>>.indexOf('222') = <<lst.indexOf('222')>>\n";
    "[1, [2, 3], 4].indexOf([2, 3]) = <<[1, [2, 3], 4].indexOf([2, 3])>>\n";
    "[1, [2, 3], 4].indexOf([3, 4]) = <<[1, [2, 3], 4].indexOf([3, 4])>>\n";

    local lst3 = ['one', 'two', 'three', 'four', 'five'];
    local lst4 = ['four', 'five', 'six', 'seven'];
    "[1, 2, 3, 4].intersect([3, 4, 5]) =
        <<sayList([1, 2, 3, 4].intersect([3, 4, 5]))>>\n";
    "lst3.intersect(lst4) = <<sayList(lst3.intersect(lst4))>>\n";
    "lst3.intersect(['three', 'six']) =
        <<sayList(lst3.intersect(['three', 'six']))>>\n";
    "['two', 'four', 'eight'].intersect(lst3) =
        <<sayList(['two', 'four', 'eight'].intersect(lst3))>>\n";
    "lst3.intersect(['six', 'seven', 'eight', 'nine', 'ten']) =
        <<sayList(lst3.intersect(['six', 'seven', 'eight', 'nine', 'ten']))
        >>\n";

    regex_tests();
}

latin1ify(str)
{
    return str.toUnicode().mapAll(
        {x: x > 255 ? '\\u<<%04x x>>' : makeString(x)}).join();
}

varfunc(...)
{
    for (local i = 1 ; i <= argcount ; ++i)
    {
        tadsSay(getArg(i));
        if (i < argcount)
            ", ";
    }
}

testToInt(str, radix?)
{
    "toInteger(<<toNumStr(str)>><<radix ? ', ' + radix : ''>>) = ";
    try
    {
        "<<radix ? toInteger(str, radix) : toInteger(str) >>\n";
    }
    catch (RuntimeError exc)
    {
        "<<exc.display>>\n";
    }
}

testToNum(str, radix?)
{
    "toNumber(<<toNumStr(str)>><<radix ? ', ' + radix : ''>>) = ";
    local n = radix ? toNumber(str, radix) : toNumber(str);
    tadsSay(n);
    if (dataType(n) == TypeObject && n.ofKind(BigNumber))
        " (BigNumber)";
    "\n";
}

toNumStr(str)
{
    if (str == nil)
        return 'nil';
    else if (str == true)
        return 'true';
    else if (dataType(str) == TypeSString)
        return '\'<<str>>\'';
    else
        return str;
}

bool2str(x)
{
    return x == true ? 'true' : x == nil ? 'nil' : x;
}

function sayList(lst)
{
    if (lst == nil)
    {
        "nil";
        return;
    }
    
    tadsSay('[');
    for (local i = 1, local cnt = lst.length() ; i <= cnt ; ++i)
    {
        tadsSay(lst[i]);
        if (i < cnt)
            tadsSay(', ');
    }
    tadsSay(']');
}

export RuntimeError;

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime Error: <<errno_>>"
    errno_ = 0
;

regex_tests()
{
    "\bRegular Expression Tests\n";

    local pat1 = '%<test%>';
    local pat2 = '([a-z]+) *%1';
    local pat3 = '([a-z]+)([0-9]+)';
    "pat1 = '<<pat1.htmlify()>>'\npat2 = '<<pat2>>'\npat3 = '<<pat3>>'\n";

    "search(pat1, 'this is a test') =
        <<sayList(rexSearch(pat1, 'this is a test'))>>\n";
    "search(pat1, 'testing some tests') =
        <<sayList(rexSearch(pat1, 'testing some tests'))>>\n";
    "search(pat1, 'testing a test run') =
        <<sayList(rexSearch(pat1, 'testing a test run'))>>\n";

    "search(pat2, 'abc def ghi') =
        <<sayList(rexSearch(pat2, 'abc def ghi'))>>\n";
    "search(pat2, 'abc def def ghi') =
        <<sayList(rexSearch(pat2, 'abc def def ghi'))>>\n";
    "group(1) = <<sayList(rexGroup(1))>>\n";

    local pat4 = '<lparen>(.*?)<space>*<alpha>+<rparen>';
    local subj4 = '/hrabes/vyhrabala (v cem) (asfd)';
    local subj4a = '/hrabes/vyhrabala (verb cem) (asfd)';
    search('pat4', pat4, subj4);
    search('pat4', pat4, subj4a);

    local pat4a = '<lparen>(.*?)(<space>*<alpha>+)<rparen>';
    search('pat4a', pat4a, subj4);
    search('pat4a', pat4a, subj4a);

    local pat4b = '<lparen>(.*?)<space>*(<alpha>+)<rparen>';
    search('pat4b', pat4b, subj4);
    search('pat4b', pat4b, subj4a);

    local pat4c = '<lparen>(|(.*?)<space>)(<alpha>+)<rparen>';
    search('pat4c', pat4c, subj4);
    search('pat4c', pat4c, subj4a);

    local pat5 = '%((|(.*) )(<alpha>+)%)';
    search('pat5', pat5, 'test (a xyz) TEST');
    search('pat5', pat5, 'test (abc xyz) TEST');

    searchLast('pat1', pat1, 'this is a test');
    searchLast('pat1', pat1, 'this is a test of rexSearchLast()');
    searchLast('pat2', pat2, 'abc def ghi');
    searchLast('pat2', pat2, 'abc def def ghi ghi');
    searchLast('pat2', pat2, 'abc def def ghi');
    searchLast('pat2', pat2, 'abc abc def ghi');

    "match(pat1, 'this is a test') = <<rexMatch(pat1, 'this is a test')>>\n";
    "match(pat1, 'test a bit') =
         <<rexMatch(pat1, 'test a bit')>>\n";
    "match(pat1, 'testing one two three') =
         <<rexMatch(pat1, 'testing one two three')>>\n";

    "match(pat3, 'abcdef123!!!') = <<rexMatch(pat3, 'abcdef123!!!')>>\n";
    "group(1) = <<sayList(rexGroup(1))>>\n";
    "group(2) = <<sayList(rexGroup(2))>>\n";

    "replace(pat1, 'this is a test', 'TEST!!!', ReplaceOnce) =
        <<rexReplace(pat1, 'this is a test', 'TEST!!!', ReplaceOnce)>>\n";
    "replace(pat3, 'this is box123!!!', '%2%1', ReplaceOnce) =
        <<rexReplace(pat3, 'this is box123!!!', '%2%1', ReplaceOnce)>>\n";
    "replace(pat2, 'abc def def ghi', '&lt;%*>&lt;%*>', ReplaceOnce) =
        <<rexReplace(pat2, 'abc def def ghi', '<%*><%*>', ReplaceOnce)
         .htmlify()>>\n";
    "replace('^[a-z]*$', 'this is a test', '%*%*', ReplaceOnce) =
        <<rexReplace('^[a-z]*$', 'this is a test', '%*%*', ReplaceOnce)>>\n";
    "replace('^[a-z]*$', 'testing', '%*%*', ReplaceOnce) =
        <<rexReplace('^[a-z]*$', 'testing', '%*%*', ReplaceOnce)>>\n";
    "replace('%([0-9][0-9][0-9]%) *[0-9][0-9][0-9]-[0-9][0-9][0-9][0-9]',
             'dial (800) 555-1212 on phone', '\"%*\"', ReplaceOnce) =
         <<rexReplace('%([0-9][0-9][0-9]%) '
                      + '*[0-9][0-9][0-9]-[0-9][0-9][0-9][0-9]',
                      'dial (800) 555-1212 on phone', '"%*"',
                      ReplaceOnce)>>\n";

    "replace('[0-9]+', 'abc 123 def 456 789 ghi', '(%*)', ReplaceAll) =
        <<rexReplace('[0-9]+', 'abc 123 def 456 789 ghi', '(%*)',
                     ReplaceAll)>>\n";
    "replace('[0-9]+', '123 def 456 789', '(%*)', ReplaceAll) =
        <<rexReplace('[0-9]+', '123 def 456 789', '(%*)',
                     ReplaceAll)>>\n";
    "replace('[0-9]+', 'abc 123 def 456 789 ghi', '(%*)', ReplaceOnce) =
        <<rexReplace('[0-9]+', 'abc 123 def 456 789 ghi', '(%*)',
                     ReplaceOnce)>>\n";
    "replace('([a-z])([0-9]+)', 'x1937y2908z4200d', '%1 := %2; ',
             ReplaceAll) =
        <<rexReplace('([a-z])([0-9]+)', 'x1937y2908z4200d', '%1 := %2; ',
                     ReplaceAll)>>\n";

    local rc = '[[[<<rexReplace(
        R'a<alpha>+', 'abc def asdf', {m: m.toUpper()})>>]]]';
    "replace with concat: <<rc>>\n";

    "\bString.find() and String.findLast() tests with rexGroup()\n";
    testFind('test one123 two456 three789 end', R'(%w+?)(%d+)');
    testFindLast('test one123 two456 three789 end', R'(%w+?)(%d+)');
}

search(name, pat, subj)
{
    "search(<<name>>='<<toString(pat).htmlify()>>', '<<subj.htmlify()>>') =
       <<sayList(rexSearch(pat, subj))>>\n";
    showRexGroups();
}

showRexGroups()
{
    for (local i = 0 ; i <= 10 ; ++i)
    {
        local g = rexGroup(i);
        if (g != nil)
            "\t%<<i>> = '<<g[3].htmlify()>>' (index=<<g[1]>>, len=<<g[2]>>)\n";
    }
}

searchLast(name, pat, subj)
{
    "searchLast(<<name>>='<<toString(pat).htmlify()>>', '<<subj.htmlify()>>') =
        <<sayList(rexSearchLast(pat, subj))>>\n";
    showRexGroups();
}

testAbs(val)
{
    "abs(<<val>>) = <<abs(val)>>\n";
}

testSgn(val)
{
    "sgn(<<val>>) = <<sgn(val)>>\n";
}

sum(a, b)
{
    return a + b;
}

testConcat([args])
{
    "concat(<<args.join(', ')>>) = <<concat(args...)>>\n";
}

testFind(subj, targ, idx?)
{
    local subjn = toString(subj).htmlify();
    if (idx == nil)
        "'<<subjn>>'.find(<<dispTarg(targ)>>) = <<subj.find(targ)>>\n";
    else
        "'<<subjn>>'.find(<<dispTarg(targ)>>, <<idx>>) = <<
          subj.find(targ, idx)>>\n";

    if (targ.ofKind(RexPattern))
        showRexGroups();
}

testFindLast(subj, targ, idx?)
{
    local subjn = toString(subj).htmlify();
    if (idx == nil)
        "'<<subjn>>'.findLast(<<dispTarg(targ)>>) = <<subj.findLast(targ)>>\n";
    else
        "'<<subjn>>'.findLast(<<dispTarg(targ)>>, <<idx>>) = <<
          subj.findLast(targ, idx)>>\n";

    if (targ.ofKind(RexPattern))
        showRexGroups();
}

dispTarg(targ)
{
    local targn = toString(targ).htmlify();
    if (targ.ofKind(RexPattern)) "R";
    "'<<targn>>'";
}

