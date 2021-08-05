#include <tads.h>

counter: object
    cnt = 0
;

echo(id, txt)
{
    "[<<++counter.cnt>>: <<id>>] <<txt>>\n";
}

main(args)
{
    local str = 'this is a test abcdef';
    str += ' ABCDEF';
    str += ' AbcDef';

    // not found
    echo('bb->xx', str.findReplace('bb', 'xx'));

    // simple replace-all tests
    echo('b->x, all', str.findReplace('b', 'x'));
    echo('b->x, all,nocase',
         str.findReplace('b', 'x',
                         ReplaceIgnoreCase));
    echo('b->x, all,nocase,follow',
         str.findReplace('b', 'x',
                         ReplaceAll | ReplaceIgnoreCase | ReplaceFollowCase));

    // same thing, with expansion
    echo('ab->xxxx, all', str.findReplace('ab', 'xxxx'));
    echo('ab->xxxx, all,nocase',
         str.findReplace('ab', 'xxxx',
                         ReplaceAll | ReplaceIgnoreCase));
    echo('ab->xxxx, all,nocase,follow',
         str.findReplace('ab', 'xxxx',
                         ReplaceIgnoreCase | ReplaceFollowCase));

    // from an index - 15, 16, 17
    for (local i = 15 ; i <= 17 ; ++i)
    {
        echo('ab->xxxx, all, from <<i>>',
             str.findReplace('ab', 'xxxx', ReplaceAll, i));
        echo('ab->xxxx, all,nocase, from <<i>>',
             str.findReplace('ab', 'xxxx',
                             ReplaceAll | ReplaceIgnoreCase, i));
        echo('ab->xxxx, all,nocase,follow, from <<i>>',
             str.findReplace('ab', 'xxxx',
                             ReplaceIgnoreCase | ReplaceFollowCase,
                             i));
    }

    // once only
    echo('ab->xxxx, once', str.findReplace('ab', 'xxxx', ReplaceOnce));
    echo('ab->xxxx, once,nocase',
         str.findReplace('ab', 'xxxx',
                         ReplaceOnce | ReplaceIgnoreCase));
    echo('ab->xxxx, once,nocase,follow',
         str.findReplace('ab', 'xxxx',
                         ReplaceOnce | ReplaceIgnoreCase | ReplaceFollowCase));

    // once only from index
    echo('ab->xxxx, once, from 17',
         str.findReplace('ab', 'xxxx', ReplaceOnce, 17));
    echo('ab->xxxx, once,nocase, from 17',
         str.findReplace('ab', 'xxxx',
                         ReplaceOnce | ReplaceIgnoreCase, 17));
    echo('ab->xxxx, once,nocase,follow, from 17',
         str.findReplace('ab', 'xxxx',
                         ReplaceOnce | ReplaceIgnoreCase | ReplaceFollowCase,
                         17));

    // serial vs parallel pattern tests
    echo('a->XX, b->YY, all',
         str.findReplace(['a', 'b'], ['XX', 'YY']));
    echo('a,b,c->XX, all',
         str.findReplace(['a', 'b', 'c'], 'XX'));
    echo('a->bb, b->cc, c->dd, parallel',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceAll));
    echo('a->bb, b->cc, c->dd, serial',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceSerial));
    echo('a->bb, b->cc, c->dd, parallel,nocase,follow',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceIgnoreCase | ReplaceFollowCase));
    echo('a->bb, b->cc, c->dd, serial,nocase,follow',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceAll | ReplaceSerial
                         | ReplaceIgnoreCase | ReplaceFollowCase));
    echo('a->bb, b->cc, c->dd, once,serial,nocase,follow',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceOnce | ReplaceSerial
                         | ReplaceIgnoreCase | ReplaceFollowCase));

    // limit tests
    echo('a->bb, b->cc, c->dd, parallel,nocase,follow, limit=0',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceIgnoreCase | ReplaceFollowCase,
                         nil, 0));
    echo('a->bb, b->cc, c->dd, parallel,nocase,follow, limit=1',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceIgnoreCase | ReplaceFollowCase,
                         nil, 1));
    echo('a->bb, b->cc, c->dd, parallel,nocase,follow, limit=2',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceIgnoreCase | ReplaceFollowCase,
                         nil, 2));
    echo('a->bb, b->cc, c->dd, parallel,nocase,follow, limit=3',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceIgnoreCase | ReplaceFollowCase,
                         nil, 3));
    echo('a->bb, b->cc, c->dd, parallel,nocase,follow, limit=4',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceIgnoreCase | ReplaceFollowCase,
                         nil, 4));
    echo('a->bb, b->cc, c->dd, serial,nocase,follow, limit=0',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceSerial | ReplaceIgnoreCase | ReplaceFollowCase,
                         nil, 0));
    echo('a->bb, b->cc, c->dd, serial,nocase,follow, limit=1',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceSerial | ReplaceIgnoreCase | ReplaceFollowCase,
                         nil, 1));
    echo('a->bb, b->cc, c->dd, serial,nocase,follow, limit=2',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceSerial | ReplaceIgnoreCase | ReplaceFollowCase,
                         nil, 2));
    echo('a->bb, b->cc, c->dd, serial,nocase,follow, limit=3',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceSerial | ReplaceIgnoreCase | ReplaceFollowCase,
                         nil, 3));
    echo('a->bb, b->cc, c->dd, serial,nocase,follow, limit=4',
         str.findReplace(['a', 'b', 'c'], ['bb', 'cc', 'dd'],
                         ReplaceSerial | ReplaceIgnoreCase | ReplaceFollowCase,
                         nil, 4));

    // callback tests
    echo('a->func(match, index, orig), all',
         str.findReplace('a', {m, i, o:
                         o.substr(i+1, 1) != ' ' ?
                         m == 'a' ? '[little A]' : '[big A]' : '[end A]'},
                         ReplaceIgnoreCase));

    echo('a->func(match), all',
         str.findReplace('a', {m: m == 'a' ? '[little A]' : '[big A]' },
                         ReplaceIgnoreCase));

    echo('a->func(match, ...), all',
         str.findReplace('a', {m, ...: '[<<m>>]' },
                         ReplaceIgnoreCase));

    try
    {
        echo('a->func(match, index, orig, extra), all',
             str.findReplace('a', {m, i, o, e: '[<<m>>]' },
                             ReplaceIgnoreCase));
    }
    catch (Exception e)
    {
        echo('a->func(match, index, orig, extra), all - caught exception',
             e.exceptionMessage);
    }

    echo('a->func(m), func(m,i), all',
         str.findReplace(['a', 'b'],
                         [{ m: '[<<m>>]'}, { m, i: '[<<m>>:<<i>>]'}],
                         ReplaceIgnoreCase));

    // '%1' (etc) in replacement is ignored when the pattern is literal text
    echo('a->%1, b->%2, c->%*, d->%%',
         str.findReplace(['a', 'b', 'c', 'd'],
                         ['%1', '%2', '%*', '%%'],
                         ReplaceIgnoreCase));
    
    // regular expression source
    local r1 = new RexPattern('<alpha>+');
    echo('<alpha>+ -> word(orig)',
         str.findReplace(r1, 'word(%*)',
                         ReplaceIgnoreCase | ReplaceFollowCase));

    // concatenation with result
    echo('concat',
         '[[[<<str.findReplace(R't<alpha>+', { m: m.toUpper() })>>]]]');
}
