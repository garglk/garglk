/*
 *   built-in function tests 
 */

#include "t3.h"
#include "tads.h"

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

    "\btoString test\n";
    "toString(123) = <<toString(123)>>\n";
    "toString(123, 8) = <<toString(123, 8)>>\n";
    "toString(123, 16) = <<toString(123, 16)>>\n";
    "toString(true) = <<toString(true)>>\n";
    "toString(nil) = <<toString(nil)>>\n";

    "toInteger test\n";
    "toInteger('nil') = <<bool2str(toInteger('nil'))>>\n";
    "toInteger('true') = <<bool2str(toInteger('true'))>>\n";
    "toInteger('123') = <<toInteger('123')>>\n";
    "toInteger('ffff', 16) = <<toInteger('ffff', 16)>>\n";
    "toInteger('123', 8) = <<toInteger('123', 8)>>\n";

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

    "find() test\n";
    "'<<str>>'.find('') = <<str.find('')>>\n";
    "'<<str>>'.find('abcdef') = <<str.find('abcdef')>>\n";
    "'<<str>>'.find('xyz') = <<str.find('xyz')>>\n";
    "'<<str>>'.find('1234567890') = <<str.find('1234567890')>>\n";
    "'<<str>>'.find('xxx') = <<str.find('xxx')>>\n";
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

varfunc(...)
{
    for (local i = 1 ; i <= argcount ; ++i)
    {
        tadsSay(getArg(i));
        if (i < argcount)
            ", ";
    }
}

bool2str(x)
{
    return x ? 'true' : 'nil';
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
}

