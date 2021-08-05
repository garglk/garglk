#include <tads.h>

counter: object
    n_ = 1
;

echo(id, str)
{
    "[<<counter.n_++>>: <<id>>] <<str>>\n";
}

main(args)
{
    local i, j;
    
    echo('all s->S', rexReplace('s', 'this is a test', 'S'));
    echo('one s->S', rexReplace('s', 'this is a test', 'S', ReplaceOnce));
    echo('all s->S from 5', rexReplace('s', 'this is a test', 'S',
                                       ReplaceAll, 5));
    echo('one s->S from 5', rexReplace('s', 'this is a test', 'S',
                                       ReplaceOnce, 5));
    echo('all s->S from 4', rexReplace('s', 'this is a test', 'S',
                                       ReplaceAll, 4));
    echo('one s->S from 4', rexReplace('s', 'this is a test', 'S',
                                       ReplaceOnce, 4));

    echo('a->x, b->y, c->z, case sensitive',
         rexReplace(['a', 'b', 'c'], 'abc ABC this is an abc ABC test',
                    ['x', 'y', 'z']));
    echo('a->x, b->y, c->z, case insensitive',
         rexReplace(['a', 'b', 'c'], 'abc ABC this is an abc ABC test',
                    ['x', 'y', 'z'], ReplaceIgnoreCase));
    echo('a->x, b->y, c->z, case insensitive, follow case',
         rexReplace(['a', 'b', 'c'], 'abc ABC this is an abc ABC test',
                    ['x', 'y', 'z'],
                    ReplaceAll | ReplaceIgnoreCase | ReplaceFollowCase));
    echo('and->und, or->oder, the->der, case sensitive',
         rexReplace(['and', 'or', 'the'],
                    'This and That AND These and Those OR '
                    + 'the Other Thing Or Whatever And Done!',
                    ['und', 'oder', 'der'],
                    ReplaceAll));
    echo('and->und, or->oder, the->der, case sensitive',
         rexReplace(['and', 'or', 'the'],
                    'This and That AND These and Those OR '
                    + 'the Other Thing Or Whatever And Done!',
                    ['und', 'oder', 'der'],
                    ReplaceAll | ReplaceIgnoreCase));
    echo('and->und, or->oder, the->der, case sensitive',
         rexReplace(['and', 'or', 'the'],
                    'This and That AND These and Those OR '
                    + 'the Other Thing Or Whatever And Done!',
                    ['und', 'oder', 'der'],
                    ReplaceIgnoreCase | ReplaceFollowCase));

    echo('digits->(digits)',
         rexReplace('(<digit>+)', '123, 4567, and 891011', '(%1)'));

    echo('all s->SS, t->TT',
         rexReplace(['s', 't'], 'this is a test', ['SS', 'TT']));
    echo('one s->SS, t->TT',
         rexReplace(['s', 't'], 'this is a test', ['SS', 'TT'],
                    ReplaceOnce));

    echo('serial s->SS, t->TT',
         rexReplace(['s', 't'], 'this is a test', ['SS', 'TT'],
         ReplaceSerial));
    echo('serial,one s->SS, t->TT',
         rexReplace(['s', 't'], 'this is a test', ['SS', 'TT'],
                    ReplaceOnce | ReplaceSerial));
    echo('serial,one q->QQ, t->TT',
         rexReplace(['q', 't'], 'this is a test', ['QQ', 'TT'],
                    ReplaceOnce | ReplaceSerial));
    echo('parallel s->SS, t->TT',
         rexReplace(['s', 't'], 'this is a test', ['SS', 'TT']));

    echo('serial a->bb, b->cc, c->dd',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'],
         ReplaceAll | ReplaceSerial));
    echo('parallel a->bb, b->cc, c->dd',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'],
         ReplaceAll));

    echo('serial a->bb, b->cc, c->dd from 5',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'],
         ReplaceAll | ReplaceSerial, 5));
    echo('parallel a->bb, b->cc, c->dd from 5',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'],
         ReplaceAll, 5));

    echo('serial a->bb, b->cc, c->dd limit=0',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'],
         ReplaceAll | ReplaceSerial, nil, 0));
    echo('serial a->bb, b->cc, c->dd limit=2',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'],
         ReplaceAll | ReplaceSerial, nil, 1));
    echo('serial a->bb, b->cc, c->dd limit=2',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'],
         ReplaceAll | ReplaceSerial, nil, 2));
    echo('serial a->bb, b->cc, c->dd limit=3',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'],
         ReplaceAll | ReplaceSerial, nil, 3));

    echo('parallel a->bb, b->cc, c->dd limit=0',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'], nil, nil, 0));
    echo('parallel a->bb, b->cc, c->dd limit=2',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'], nil, nil, 1));
    echo('parallel a->bb, b->cc, c->dd limit=2',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'], nil, nil, 2));
    echo('parallel a->bb, b->cc, c->dd limit=3',
         rexReplace(['a', 'b', 'c'], 'abc this is an abc test',
                    ['bb', 'cc', 'dd'], nil, nil, 3));

    i = 1;
    echo('alpha->func(even \'s\' only)',
         rexReplace('<alpha>', 'this is a test of some special stuff',
                    { match, idx, orig: match == 's' && (i++ & 1) == 0
                                        ? match.toUpper() : match }));

    i = 1;
    echo('t,s->func(even only)',
         rexReplace(['s', 't'], 'this is a test of some special stuff',
                    { match, idx, orig: (i++ & 1) == 0
                                        ? match.toUpper() : match }));

    i = j = 1;
    echo('s->func(even only), t->func(odd only)',
         rexReplace(['s', 't'], 'this is a test of some special stuff',
                    [{ m, idx, o: (i++ & 1) == 0 ? m.toUpper() : m },
                     { m, idx, o: (j++ & 1) != 0 ? m.toUpper() : m }]));

    echo('s,t->func(m), func(m,i)',
         rexReplace(['s', 't'], 'this is a test of some special stuff',
                    [{ m: '[<<m>>]'}, { m, i: '[<<m>>:<<i>>]' }]));
}
