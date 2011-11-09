#include "dict.h"
#include "gramprod.h"
#include "tads.h"
#include "t3.h"
#include "tok.h"

main(args)
{
    "Test.  Type 'quit' to exit.\b";

    for (;;)
    {
        local str, toks, matches;

                ">";
                str = inputLine();
                try { toks = Tokenizer.tokenize(str); }
                catch (TokErrorNoMatch e) { continue; }

                if (toks.length() == 1 && toks[1][1] == 'quit') break;

                matches = command.parseTokens(toks, nil);
                "Matches: <<matches.length()>>\b";
        }
}

grammar command: 'a' | 'b' ('c' | 'd'): object;

