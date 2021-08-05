#include <tads.h>

main(args)
{
    test('one,two,three', ',');
    test('one,two,three', ',', 1);
    test('one,two,three', ',', 3);
    test('one,two,three', ',', 10);
    test('one,two,three,four,five,six,seven,eight,nine,ten,eleven,twelve,'
         + 'thirteen,fourteen,fifteen,sixteen,seventeen,eighteen,'
         + 'nineteen,twenty,twenty-one,twenty-two,twenty-three', ',');
    test('one,two,three,four,five,six,seven,eight,nine,ten,eleven,twelve,'
         + 'thirteen,fourteen,fifteen,sixteen,seventeen,eighteen,'
         + 'nineteen,twenty,twenty-one,twenty-two,twenty-three', ',', 5);
    test('one,two,three,four,five,six,seven,eight,nine,ten,eleven,twelve,'
         + 'thirteen,fourteen,fifteen,sixteen,seventeen,eighteen,'
         + 'nineteen,twenty,twenty-one,twenty-two,twenty-three', ',', 16);
    test('one,two,three,four,five,six,seven,eight,nine,ten,eleven,twelve,'
         + 'thirteen,fourteen,fifteen,sixteen,seventeen,eighteen,'
         + 'nineteen,twenty,twenty-one,twenty-two,twenty-three', ',', 30);

    testi('abcdefghi');
    testi('abcdefghi', 2);
    testi('abcdefghi', 3);
    testi('abcdefghi', 3, 2);
    testi('abcdefghi', 3, 3);
    testi('abcdefghi', 22);

    testp('one,two, three,  four', ',<space>*');
    testp('one,two, three,  four,   five, six', ',<space>*', 3);
    testp('one,two, three,  four, five,   six', ',<space>*', 5);
}

testi(str, delim?, limit?)
{
    "'<<str.htmlify()>>'.split(<<delim>>, <<limit>>) -&gt; <<
      sayList(str.split(delim, limit))>>\b";
}

test(str, delim?, limit?)
{
    local delimStr = (delim.ofKind(RexPattern)
                      ? '/<<delim.getPatternString()>>/' : delim);
    "'<<str.htmlify()>>'.split('<<delimStr.htmlify()>>', <<limit>>) -&gt; <<
      sayList(str.split(delim, limit))>>\b";
}

testp(str, delim, limit?)
{
    test(str, new RexPattern(delim), limit);
}

sayList(lst)
{
    "[";
    for (local i = 1, local len = lst.length() ; i <= len ; ++i)
    {
        if (i > 1)
            ", ";
        "'<<lst[i]>>'";
    }
    "]";
}

