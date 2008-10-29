/*
 *   grammar test
 */

#include "tads.h"
#include "t3.h"
#include "dict.h"
#include "gramprod.h"
#include "tok.h"


dictionary gDict;
dictionary property noun, adjective, plural;

/* copied from _main.t, which we don't include in this test */
dataTypeXlat(val)
{
    /* get the base type */
    local t = dataType(val);
    
    /* if it's an anonymous function, return TypeFuncPtr */
    if (t == TypeObject && val.ofKind(AnonFuncPtr))
        return TypeFuncPtr;

    /* otherwise, just return the base type */
    return t;
}

indent(level)
{
    for ( ; level != 0 ; --level)
        "\ \ ";
}

grammar command: predicate->pred_
    | predicate->pred_ cmdTerminator * : object
    debugPrint(level)
        { indent(level); "command: pred\n"; pred_.debugPrint(level+1); }
;

grammar command: cmdTerminator * : object
    debugPrint(level) { indent(level); "command (empty)\n"; }
;   

grammar cmdTerminator: 'then' | '.' | '!' | ';' | '?' | ',' | 'and' : object
;

grammar predicate: 'say' tokString->str_ : object
    debugPrint(level)
        { indent(level); "predicate say(str_ = <<str_>>)\n"; }
    execute()
    {
        "Okay, \"<<str_>>\"\n";
    }
;

grammar predicate: 'take' nounPhrase->np_ : object
    debugPrint(level)
        { indent(level); "predicate take np:\n"; np_.debugPrint(level+1); }
;

grammar predicate: 'give' nounPhrase->dobj_ 'to' nounPhrase->iobj_
                 | 'give' nounPhrase->iobj_ nounPhrase->dobj_ : object
    debugPrint(level)
    {
        indent(level);
        "predicate give dobj, iobj:\n";
        dobj_.debugPrint(level+1);
        iobj_.debugPrint(level+1);
    }
;

grammar nounPhrase: noun->noun_ : object
    debugPrint(level)
        { indent(level); "nounPhrase(noun = <<noun_>>)\n"; }
    resolveObjects()
    {
        /* return the objects matching my noun */
        return gDict.findWord(noun_, &noun);
    }
;

grammar nounPhrase: [badness 0+10] anyPhrase->any_: object
    debugPrint(level)
    {
        indent(level);
        "nounPhrase any:\n";
        any_.debugPrint(level + 1);
    }
;

grammar nounPhrase: adjective->adj_ nounPhrase->np_ : object
    debugPrint(level)
    {
        indent(level);
        "nounPhrase(adj = <<adj_>>) np:\n";
        np_.debugPrint(level+1);
    }
    resolveObjects()
    {
        local match1;
        local match2;
        
        /* get the objects matching my adjective */
        match1 = gDict.findWord(adj_, &adjective);

        /* get the objects matching the rest of the noun phrase */
        match2 = np_.resolveObjects();

        np_.adj_ = true;//$$$ test only - assign property of variable
        match1.adj_ = true;

        /* intersect the lists to yield objects matching all words */
        return match1.intersect(match2);
    }
;

grammar anyPhrase: tokWord->txt_: object
    debugPrint(level) { indent(level); "anyPhrase(<<txt_>>)\n"; }
;

grammar anyPhrase: tokWord->txt_ anyPhrase->phrase_: object
    debugPrint(level)
    {
        indent(level);
        "anyPhrase(<<txt_>>) phrase:\n";
        phrase_.debugPrint(level + 1);
    }
;

class Item: object
;

redBall: Item noun = 'ball' adjective = 'red';
blueBall: Item noun = 'ball' adjective = 'blue';

main(args)
{
    local cmds =
    [
        'take red ball',
        'give red ball to blue ball',
        'give red ball blue ball',
        'say "hello there!"',
        'say "that\'s about all" and take ball'
    ];
    local curcmd;
        
    for (curcmd = 1 ; curcmd <= cmds.length() ; ++curcmd)
    {
        local str, toks;
        local match;

        /* display the next input line */
        str = cmds[curcmd];
        "><<str>>\n";

        /* tokenize the string */
        toks = Tokenizer.tokenize(str);

        /* if it's 'quit' or 'q', stop */
        if (toks.length() == 1
            && (getTokVal(toks[1]) is in ('q', 'quit')))
            break;

        /* keep going until we parse all commands in the string */
        for (;;)
        {
            local used;
            
            /* parse it */
            match = command.parseTokens(toks, gDict);

            /* if we didn't get anything, say so */
            if (match.length() == 0)
            {
                "That command is not recognized. ";
                break;
            }

            /* display the matches */
            for (local i = 1, local cnt = match.length() ; i <= cnt ; ++i)
            {
                /* get the token count */
                used = match[i].lastTokenIndex - match[i].firstTokenIndex + 1;

                /* display this match */
                "[match <<i>>: token count = <<used>>\n";
                match[i].debugPrint(1);
                "\b";
            }

            /* if more follows, move on */
            if (used < toks.length())
            {
                /* remove the used parts from the lists */
                toks = toks.sublist(used + 1);
            }
            else
            {
                /* we're done */
                break;
            }
        }
    }
}

preinit()
{
}

/* ------------------------------------------------------------------------ */
/*
 *   some boilerplate setup stuff 
 */

class Exception: object
    display = "Unknown exception"
;

class RuntimeError: Exception
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime error: <<exceptionMessage>>"
    errno_ = 0
    exceptionMessage = ''
;

_say_embed(str) { tadsSay(str); }

_main(args)
{
    try
    {
        t3SetSay(_say_embed);
        if (!global.preinited_)
        {
            preinit();
            global.preinited_ = true;
        }
        if (!t3GetVMPreinitMode())
            main(args);
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

global: object
    preinited_ = nil
;
