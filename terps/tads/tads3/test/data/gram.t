/*
 *   grammar test
 */

#include "tads.h"
#include "t3.h"
#include "dict.h"
#include "gramprod.h"


dictionary gDict;
dictionary property noun, adjective, plural;

indent(level)
{
    for ( ; level != 0 ; --level)
        "\ \ ";
}

grammar command: predicate->pred_
    | predicate->pred_ <cmdTerminator> * : object
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
        { indent(level); "predicate tadsSay(str_ = <<str_>>)\n"; }
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
flashlight: Item noun = 'flashlight' ;

main(args)
{
    "Hello from the parser test!\b";
    for (;;)
    {
        local str, toks;
        local match;

        /* read a line */
        "\b>";
        str = inputLine();

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
                /* display this match */
                "[match <<i>>: token count = <<match[i][1]>>\n";
                match[i][2].debugPrint(1);
                "\b";

                /* proceed from here */
                used = match[i][1];
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
