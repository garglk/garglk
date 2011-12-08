#include <tads.h>

main(args)
{
    local strs = [
        'This is a test of the specialsToHtml() conversion process.',
        'These first strings have no special characters, but we\'ll get '
        + 'to that presently.',
        'We have some \ \ quoted spaces \ \ in this one.',
        'Mixing \  \  quoted and \ \   \   unquoted spaces.',
        'This one has some <q>quote <q>markups</q> end quote</q>.',
        'Let\'s try <q>quote ',
        '<q>markups</q>',
        'end quote</q> across elements!',
        'Some newline conversions: newline\nblank\bthree newlines\n\n\n'
        + 'three blanks\b\b\bnewline-blank-newline-blank\n\b\n\b',
        'A tag across lines: <a href="test',
        '<link>">hyperlinked text here</',
        'a>',
        'Caps flag: \^<q>in quotes</q>, nocaps: \v<a href=\'link',
        '-<across>-lines\'>ALL CAPS</a>',
        'BR with HEIGHT attributes... first, height=0<br height=0>\n'
        + 'Now height=1<br height=1>'
        + 'And height=2<br height=2>'
        + 'And now a height=0 at the start of a line\n<br height=0>'
        + 'And height=2 at start of line\n<br height=2>',
        'Tabs:\none\ttwo\tthree\tfour\tfive\tsix\n'
        + '\ta\tb\tc\td\n'
        + 'hello\tgoodbye\tthat should be all'
    ];

    local st = new TadsObject();
    foreach (local str in strs)
    {
        "Src:\ \ <<quoteAll(str)>><br>
        Disp: <<str.htmlify()>><br>
        s2h:\ \ <<quoteAll(str.specialsToHtml(st))>><br><br>";
    }
}

quoteAll(str)
{
    return str.htmlify().findReplace(
        ['\n', '\b', '\t', ' ', '\ ', '\v', '\^'],
        ['\\n', '\\b', '\\t', '\ ', '\\\ ', '\\v', '\\^']);
}
