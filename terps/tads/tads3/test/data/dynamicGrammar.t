#include <tads.h>
#include <gramprod.h>
#include <tok.h>

dictionary cmdDict;
dictionary property fname;

sinFunc: object
    fname = 'sin' 'sine'
;
cosFunc: object
    fname = 'cos'
;
tanFunc: object
    fname = 'tan'
;

class ExprNode: DynamicProd
    display()
    {
        if (t1 != nil)
            "<<t1.display()>> ";
        if (t2 != nil)
            "<<t2.display()>> ";
        if (op != nil)
            "<<op>>";
    }

    t1 = nil
    t2 = nil
    op = nil
;

class ExprNode2: ExprNode
    display()
    {
        "[<<grammarTag>>:ExprNode2 <<inherited()>>]";
    }
;

class AtomNode2: ExprNode
    display()
    {
        "[<<grammarTag>>:AtomNode2 <<n>>]";
    }
;

class FuncNode2: ExprNode
    display()
    {
        "[<<grammarTag>>:FuncNode2 <<e.display()>> <<f>>]";
    }
    e = nil
    f = nil
;

grammar expr(main): term->t1 : ExprNode;

grammar term(factor): factor->t1 : ExprNode;

grammar term(expr): term->t1 ('+'->op | '-'->op) factor->t2 : ExprNode;

grammar factor(pow): pow0->t1 : ExprNode;

grammar factor(expr): factor->t1 ('*'->op | '/'->op) pow0->t2 : ExprNode;

grammar pow0(expr): 'test only' : ExprNode;
#if 0 // we add 'pow' as an entirely dynamic node now!
grammar pow(atom): atom->t1 : ExprNode;
grammar pow(expr): pow->t1 '^'->op atom->t2 : ExprNode;
#endif

grammar atom(int): tokInt->n : object
    display() { "<<n>>"; }
;

grammar atom(parens): '(' expr->t1 ')' : ExprNode;

tokenizer: Tokenizer
;
enum token tokOp;

class testClass: ExprNode
    display() { "[<<id>>]"; }
    grammarTag = 'pi/e'
    id = nil
;

symtab: PreinitObject
    execute()
    {
        tab = t3GetGlobalSymbols();
        revtab = new LookupTable(256, 512);
        tab.forEachAssoc({key, val: revtab[val] = key});
    }
    tab = nil
    revtab = nil
;

PreinitObject
    execute()
    {
        local m;

        local pow = new GrammarProd();
        symtab.tab['pow'] = pow;

        m = new ExprNode2;
        m.grammarTag = 'pow(atom)';
        pow.addAlt('atom->t1', m, cmdDict, symtab.tab);

        m = new ExprNode2;
        m.grammarTag = 'pow(expr)';
        pow.addAlt('pow->t1 "^"->op atom->t2', m, cmdDict, symtab.tab);

        pow0.clearAlts(cmdDict);
        m = new ExprNode2;
        m.grammarTag = 'pow0(pow)';
        pow0.addAlt('pow->t1', m, cmdDict, symtab.tab);

        m = new ExprNode2();
        m.grammarTag = 'new[atom]';
        pow.addAlt('atom->t1', m, cmdDict, symtab.tab);

        m = new AtomNode2();
        m.grammarTag = 'new[string]';
        atom.addAlt('tokString->n', m, cmdDict, symtab.tab);

        m = new FuncNode2;
        m.grammarTag = 'new[fname]';
        atom.addAlt('fname->f "(" expr->e ")"', m, cmdDict, symtab.tab);
    }
;

main(args)
{
    tokenizer.insertRule(
        ['operator', new RexPattern('[-^+*/()]'), tokOp, nil, nil],
         'punctuation', nil);
    
    atom.addAlt('"pi"->id | "e"->id', testClass, cmdDict, symtab.tab);
    
#ifdef INTERACTIVE
    /* interactive command line version */
    for (;;)
    {
        "&gt;";
        local l = inputLine();
        if (l == nil)
            break;

        process(l);
    }

#else /* INTERACTIVE */
    /* automated testing version */
    "Dictionary contents:\n";
    cmdDict.forEachWord(
        {obj, str, prop: "'<<str>>' (<<
              symtab.revtab[obj]>>.<<symtab.revtab[prop]>>)\n"});
    "\b";
    
    process('3*4*5+6*7*8');
    process('sin(pi^e)');

#endif /* INTERACTIVE */
}

process(l)
{
    local t = tokenizer.tokenize(l);
    local lst = expr.parseTokens(t, cmdDict);
    
    if (lst.length() == 0)
        "Parsing error.\n";
    else
    {
        lst[1].display();
        "\n";
        showGrammar(lst[1]);
    }
    "\b";
}

showGrammar(match, indent = 0)
{
    if (match != nil)
    {
        "<<makeString('\ ', indent)>>";

        switch (dataType(match))
        {
        case TypeSString:
            "'<<match>>'\n";
            break;

        case TypeObject:
            local info = match.grammarInfo();
            "<<info[1]>> [<<showGrammarText(match)>>]\n";
            for (local i = 2 ; i <= info.length() ; ++i)
                showGrammar(info[i], indent + 1);
            break;
        }
    }
}

showGrammarText(match)
{
    for (local i = match.firstTokenIndex ; i <= match.lastTokenIndex ; ++i)
    {
        if (i > match.firstTokenIndex)
            " ";
        "<<getTokOrig(match.tokenList[i])>>";
    }
}
