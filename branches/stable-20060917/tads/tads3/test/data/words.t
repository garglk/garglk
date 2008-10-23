/*
 *   dictionary test 
 */

#include "tads.h"
#include "t3.h"
#include "dict.h"

class RuntimeError: object
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
        t3SetSay(&_say_embed);
        main(args);
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

obj1: object
    adjective = 'none'
    noun = 'none'
;

bluebook: object sdesc = "blue_book";
redbook: object sdesc = "red_book";
greenbook: object sdesc = "green_book";
blueball: object sdesc = "blue_ball";
redball: object sdesc = "red_ball";
greenball: object sdesc = "green_ball";

main(args)
{
    local dict;
    local lst;

    /* create a dictionary */
    dict = new Dictionary(6);

    /* add some words */
    dict.addWord(bluebook, 'blue', &adjective);
    dict.addWord(redbook, 'red', &adjective);
    dict.addWord(greenbook, 'green', &adjective);

    dict.addWord(blueball, 'blue', &adjective);
    dict.addWord(redball, 'red', &adjective);
    dict.addWord(greenball, 'green', &adjective);

    dict.addWord(bluebook, 'book', &noun);
    dict.addWord(redbook, 'book', &noun);
    dict.addWord(greenbook, 'book', &noun);

    dict.addWord(blueball, 'ball', &noun);
    dict.addWord(redball, 'ball', &noun);
    dict.addWord(greenball, 'ball', &noun);

    dict.addWord(redball, ['rouge', 'reddish', 'ruddy', 'rosy'], &adjective);

    /* look up some words */
    "noun 'book': <<sayList(dict.findWord('book', &noun))>>\n";
    "noun: 'ball': <<sayList(dict.findWord('ball', &noun))>>\n";
    "adjective: 'red': <<sayList(dict.findWord('red', &adjective))>>\n";
    "adjective: 'blue': <<sayList(dict.findWord('blue', &adjective))>>\n";
    "adjective: 'green': <<sayList(dict.findWord('green', &adjective))>>\n";
    "adjective: 'rouge':
        <<sayList(dict.findWord('rouge', &adjective))>>\n";
    "adjective: 'reddish':
        <<sayList(dict.findWord('reddish', &adjective))>>\n";
    "adjective: 'ruddy':
        <<sayList(dict.findWord('ruddy', &adjective))>>\n";
    "adjective: 'rosy':
        <<sayList(dict.findWord('rosy', &adjective))>>\n";

    /* shorten the truncation length */
    dict.setTruncLen(3);

    "\bAfter setting truncation length to 3:\n";
    "noun 'book': <<sayList(dict.findWord('book', &noun))>>\n";
    "noun: 'ball': <<sayList(dict.findWord('ball', &noun))>>\n";
    "adjective: 'red': <<sayList(dict.findWord('red', &adjective))>>\n";
    "adjective: 'blue': <<sayList(dict.findWord('blue', &adjective))>>\n";
    "adjective: 'green': <<sayList(dict.findWord('green', &adjective))>>\n";

    "noun: 'boo': <<sayList(dict.findWord('boo', &noun))>>\n";
    "noun: 'boop': <<sayList(dict.findWord('boop', &noun))>>\n";
    "adjective: 'green': <<sayList(dict.findWord('green', &adjective))>>\n";
    "adjective: 'gree': <<sayList(dict.findWord('gree', &adjective))>>\n";
    "adjective: 'gre': <<sayList(dict.findWord('gre', &adjective))>>\n";
    "adjective: 'gr': <<sayList(dict.findWord('gr', &adjective))>>\n";
    "adjective: 'greek': <<sayList(dict.findWord('greek', &adjective))>>\n";
    "adjective: 'greeny': <<sayList(dict.findWord('greeny', &adjective))>>\n";

    "\bFinding with truncation:\n";
    "noun: 'boo': <<sayList(dict.findWordTrunc('boo', &noun))>>\n";
    "noun: 'boop': <<sayList(dict.findWordTrunc('boop', &noun))>>\n";
    "adjective: 'green':
        <<sayList(dict.findWordTrunc('green', &adjective))>>\n";
    "adjective: 'gree':
        <<sayList(dict.findWordTrunc('gree', &adjective))>>\n";
    "adjective: 'gre': <<sayList(dict.findWordTrunc('gre', &adjective))>>\n";
    "adjective: 'gr': <<sayList(dict.findWordTrunc('gr', &adjective))>>\n";
    "adjective: 'greek':
        <<sayList(dict.findWordTrunc('greek', &adjective))>>\n";
    "adjective: 'greeny':
        <<sayList(dict.findWordTrunc('greeny', &adjective))>>\n";
}

sayList(lst)
{
    "[";
    for (local i = 1, local len = lst.length() ; i <= len ; ++i)
    {
        if (i > 1)
            ", ";
        lst[i].sdesc;
    }
    "]";
}
