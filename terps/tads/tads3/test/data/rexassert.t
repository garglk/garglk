#include <tads.h>

main(args)
{
    test('<nocase>%<[a-z]+%>(?<!%<a+%>)', 'a aaa aaaaa aaba aa aaaa');
    test('s(?!<space>)', 'this is a test string');
    test('(?<!<space|i>)s', 'this is a silly test string');
    test('(?<!\\)"',
         'A string with \\"slashed\\" quotes and one-"-non-slashed');
    test('<nocase>[a-z]+<space>+(?<!test<space>+)word',
         'this is a test word, another test word, a non-test regular word');
    test('<nocase>[a-z]+<space>+(?<!a test<space>+)word',
         'this is a test word, another test word, a non-test regular word');
    test('(?<=^|[.!?]<space>+)%<%w+%>', 'This is our test. It\'s very nice.');
    test('(?<=[.!?]<space>+)%<%w+%>', 'This is our test. It\'s very nice.');

    // These are to test loop detection.  Assertions don't consume any
    // input, so applying * to an assertion or group of assertion basically
    // renders the assertion meaningless; and applying + means we should
    // just execute the assertion once.
    test('(?=x)*.*', 'asdf');  // * makes the assertion irrelevant
    test('((?=x))*.*', 'asdf');  // * makes the assertion irrelevant
    test('(?=x)+.*', 'asdf');  // + keeps the assertion
    test('((?=x))+.*', 'asdf');  // + keeps the assertion
    test('(?=x)+.*', 'xyz');   // + keeps the assertion
    test('((?=x))+.*', 'xyz');   // + keeps the assertion
    test('(((?=x)(?=x)(?=x))*)+.*', 'asdf');  // * drops the assertion
    test('(((?=x)(?=x)(?=x))+)*.*', 'asdf');  // * drops the assertion
    test('(((?=x)(?=x)(?=x))+)+.*', 'asdf');  // + keeps it
    test('(((?=x)(?=x)(?=x))+)+.*', 'xyz');  // + keeps it
}

test(pat, str)
{
    "<<pat.htmlify()>><br><<str>><br>";
    local match = rexSearch(pat, str);
    if (match)
    {
        "<<makeString('\ ', match[1]-1)>><<makeString('^', match[2])>><br>";
        "index=<<match[1]>>, length=<<match[2]>>, match=<<match[3]>><br>";
    }
    else
        "nil<br>";
    "<br>";
}

