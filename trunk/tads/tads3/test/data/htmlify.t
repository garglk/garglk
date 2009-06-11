#include "tads.h"
#include "t3.h"

main(args)
{
    say_htmlify('This is a <test> of htmlify!');
    say_htmlify('& this is another!');
    say_htmlify('tab\ttab\ttab (not keeping tabs)');
    say_htmlify('tab\ttab\ttab\t(keeping tabs)', HtmlifyTranslateTabs);
    say_htmlify('newline\nnewline\nnewline (not keeping newlines)');
    say_htmlify('newline\n\ttab, newline\n\ttab (keeping newlines and tabs)',
                HtmlifyTranslateTabs | HtmlifyTranslateNewlines);
    say_htmlify('lots    of    spaces');
    say_htmlify('keeping     lots    of    spaces', HtmlifyTranslateSpaces);
}

say_htmlify(str, [flags])
{
    tadsSay(str.htmlify(flags...).htmlify());
    tadsSay('\n');
}

