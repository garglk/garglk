#include <tads.h>
#include <strbuf.h>

globals: object
    prop1 = nil
;

main(args)
{
    if (args.length() > 1 && args[2] == '-restore')
    {
        "Restoring...\n";
        restoreGame('strbuf.t3v');
        
        "globals.prop1 = <<globals.prop1>>\n";
        return;
    }
    
    local s = new StringBuffer();

    s.append('Testing ');
    s.append('one... ');
    s.append('two... ');
    s.append('three... ');
    "[1] s=[<<s>>]\n";

    s.deleteChars(1, 8);
    "[2] s=[<<s>>]\n";

    s.deleteChars(8, 3);
    "[3] s=[<<s>>]\n";

    s.insert(8, 'TWO!!!');
    "[4] s=[<<s>>]\n";

    s.splice(11, 3, '@@@@@');
    "[5] s=[<<s>>]\n";

    s[4] = '*';
    s[5] = 35;
    s[6] = 43;
    s[-1] = '$';
    "[6] s=[<<s>>]\n";

    for (local i = 1 ; i <= 100 ; ++i)
    {
        s.append('(i is now ');
        s.append(i);
        s.append('...) ');
    }
    "[7] s=[<<s>>]\n";

    s.deleteChars(s.length(), 1);
    "[8] s=[<<s>>]\n";

    "\b[9] s.substr(8,11) = [<<s.substr(8, 11)>>]\n";
    "[10] s.substr(s.length()-10) = [<<s.substr(s.length() - 10)>>]\n";
    "[11] s.substr(-10) = [<<s.substr(-10)>>]\n";
    "[12] s.substr(-11, 5) = [<<s.substr(-11, 5)>>]\n";

    "[13] s.charAt(1) = <<s.charAt(1)>>\n";
    "[14] s.charAt(2) = <<s.charAt(2)>>\n";
    "[15] s.charAt(-1) = <<s.charAt(-1)>>\n";
    "[16] s.charAt(-2) = <<s.charAt(-2)>>\n";

    "\bUndo tests:\b";

    savepoint();

    s.deleteChars(1);
    "[u1] s=[<<s>>]\n";

    undo();
    "[u2] s=[<<s>>]\n";

    s.deleteChars(1);
    savepoint();

    s.append('Testing ');
    savepoint();
    
    s.append('one... ');
    savepoint();
    
    s.append('two... ');
    savepoint();

    s.append('three... ');
    "[u3] s=[<<s>>]\n";

    undo();
    "[u4] s=[<<s>>]\n";

    undo();
    "[u5] s=[<<s>>]\n";

    undo();
    "[u6] s=[<<s>>]\n";

    undo();
    "[u7] s=[<<s>>]\n";

    s.append('Testing one... two... three...');
    savepoint();
    "[u8] s=[<<s>>]\n";

    s.deleteChars(9, 6);
    "[u9] s=[<<s>>]\n";

    undo();
    "[u10] s=[<<s>>]\n";

    savepoint();
    s.splice(9, 6, '{SPLICE HERE!!!}');
    "[u11] s=[<<s>>]\n";

    undo();
    "[u12] s=[<<s>>]\n";

    savepoint();
    s.insert(9, '{INSERT HERE!!!}');
    "[u13] s=[<<s>>]\n";

    undo();
    "[u14] s=[<<s>>]\n";

    savepoint();
    s[12] = '*';
    s[13] = '+';
    s[14] = '#';
    "[u15] s=[<<s>>]\n";

    undo();
    "[u16] s=[<<s>>]\n";

    savepoint();
    s.copyChars(13, 'ABCDE');
    "[u17] s=[<<s>>]\n";
    undo();
    "[u18] s=[<<s>>]\n";

    "\bComparison tests\b";

    s.deleteChars(1);
    s.append('Hello ');
    s.append('there!');

    local str = 'Hello';
    str += ' there!';
    "[c1] s=[<<s>>]\n
    s == 'Hello there!'? <<yesno(s == 'Hello there!')>>\n
    s == str? <<yesno(s == str)>>\n
    'Hello there!' == s ? <<yesno('Hello there!' == s)>>\n
    str == s? <<yesno(str == s)>>\n
    s > 'Hello there!'? <<yesno(s > 'Hello there!')>>\n
    s &lt; 'Hello there!'? <<yesno(s < 'Hello there!')>>\n
    s > str? <<yesno(s > str)>>\n
    s &lt; str? <<yesno(s < str)>>\n
    'Hello there!' > s? <<yesno('Hello there!' > s)>>\n
    'Hello there!' &lt; s? <<yesno('Hello there!' < s)>>\n
    str > s? <<yesno(str > s)>>\n
    str &lt; s? <<yesno(str < s)>>\b";

    s.deleteChars(1);
    s.append('Hello ');
    s.append('here!');
    "[c2] s=[<<s>>]\n
    s == 'Hello there!'? <<yesno(s == 'Hello there!')>>\n
    s == str? <<yesno(s == str)>>\n
    'Hello there!' == s ? <<yesno('Hello there!' == s)>>\n
    str == s? <<yesno(str == s)>>\n
    s > 'Hello there!'? <<yesno(s > 'Hello there!')>>\n
    s &lt; 'Hello there!'? <<yesno(s < 'Hello there!')>>\n
    s > str? <<yesno(s > str)>>\n
    s &lt; str? <<yesno(s < str)>>\n
    'Hello there!' > s? <<yesno('Hello there!' > s)>>\n
    'Hello there!' &lt; s? <<yesno('Hello there!' < s)>>\n
    str > s? <<yesno(str > s)>>\n
    str &lt; s? <<yesno(str < s)>>\b";

    s.insert(7, 't');
    str = new StringBuffer(64, 64);
    str.append('Hello ');
    str.append('there!');
    "[c4] s=[<<s>>], str=[<<str>>]\n
    s == str? <<yesno(s == str)>>\n
    str == s? <<yesno(str == s)>>\n
    s > str? <<yesno(s > str)>>\n
    s &lt; str? <<yesno(s < str)>>\n
    str > s? <<yesno(str > s)>>\n
    str &lt; s? <<yesno(str < s)>>\b";

    s.deleteChars(7, 1);
    "[c5] s=[<<s>>], str=[<<str>>]\n
    s == str? <<yesno(s == str)>>\n
    str == s? <<yesno(str == s)>>\n
    s > str? <<yesno(s > str)>>\n
    s &lt; str? <<yesno(s < str)>>\n
    str > s? <<yesno(str > s)>>\n
    str &lt; s? <<yesno(str < s)>>\b";

    globals.prop1 = s;
    saveGame('strbuf.t3v');
}

yesno(val) { return val ? 'yes' : 'no'; }

