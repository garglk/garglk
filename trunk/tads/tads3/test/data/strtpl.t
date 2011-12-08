/*
 *   String template tests 
 */

#include <tads.h>

string template <<* in words>> inWords;
string template <<twice *>> twice;
string template <<thrice *>> thrice;
string template <<player location>> playerLoc;
string template <<*>> embed;

twice(x)
{
    return x * 2;
}

thrice(x)
{
    return x * 3;
}

playerLoc()
{
    return 'In the Kitchen';
}

embed(x)
{
    if (dataType(x) == TypeObject && x.ofKind(Thing))
        return x.name;

    return x;
}

class Thing: object
    name = 'thing'
;

book: Thing
    name = 'blue book'
;

inWords(x)
{
    local small = ['one', 'two', 'three', 'four', 'five', 'six', 'seven',
                   'eight', 'nine', 'ten', 'eleven', 'twelve', 'thirteen',
                   'fourteen', 'fifteen', 'sixteen', 'seventeen',
                   'eighteen', 'nineteen'];
    local tens = ['twenty', 'thirty', 'forty', 'fifty', 'sixty',
                  'seventy', 'eighty', 'ninety'];

    if (x < 0)
        return 'minus <<inWords(-x)>>';
    if (x == 0)
        return 'zero';

    local ret = '', q;
    if (x > 1000000000)
    {
        q = x/1000000000;
        x %= 1000000000;
        ret += '<<q in words>> billion ';
    }
    if (x > 1000000)
    {
        q = x/1000000;
        x %= 1000000;
        ret += '<<q in words>> million ';
    }
    if (x > 1000)
    {
        q = x/1000;
        x %= 1000;
        ret += '<<q in words>> thousand ';
    }
    if (x > 100)
    {
        q = x/100;
        x %= 100;
        ret += '<<small[q]>> hundred ';
    }
    if (x > 20)
    {
        q = x/10;
        x %= 10;
        ret += tens[q-1];
        if (x != 0)
            ret += '-<<small[x]>>';
    }
    else
    {
        ret += small[x];
    }

    if (ret.endsWith(' '))
        ret = ret.substr(1, -1);

    return ret;
}


main(args)
{
    local a = 123;
    "Let's try some tests: twice a is <<twice a>>, twice a in words
    is <<twice a in words>>, thrice f(a) is <<thrice f(a)>>, thrice f(a) in
    words is <<thrice f(a) in words>>! By the way, the player's location
    is <<player location>>. And finally, book=<<book>>.\n";
}

f(x)
{
    return x*1234567;
}

