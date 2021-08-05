/*
 *   test of part-of-speech lists in grammar definitions 
 */

#include <tads.h>
#include <gramprod.h>
#include <tok.h>

dictionary gDict;

dictionary property nounM, nounF, nounN;

bookM: object
    desc = 'book'
    nounM = 'book'
;

boxF: object
    desc = 'box'
    nounF = 'box'
;

bagN: object
    desc = 'bag'
    nounN = 'bag'
;

grammar nounPhrase: <nounM nounF>->noun_ : object
    desc = "nounPhrase<M/F>: noun = <<noun_>>"
;

grammar nounPhrase: nounN->noun_ : object
    desc = "nounPhrase<N>: noun = <<noun_>>"
;

main(args)
{
    "Enter an empty line to quit.\b";

    /* keep going until done */
    for (;;)
    {
        local str;
        local match;
        
        /* read a line */
        "Type a noun: ";
        str = inputLine();

        /* if it's empty, quit */
        if (str == '' || str == nil)
            break;

        /* parse it */
        match = nounPhrase.parseTokens(Tokenizer.tokenize(str), gDict);

        /* show any matches */
        foreach (local cur in match)
        {
            cur.desc;
            "\n";
        }
    }
}

