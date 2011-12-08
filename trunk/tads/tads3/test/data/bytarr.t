#include <tads.h>
#include <bytearr.h>

main(args)
{
    local b = new ByteArray(1024*1024);
    local c;
    local i;
    local cset;

    b[32767] = 1;
    b[32768] = 2;
    b[32769] = 3;

    for (i = 640*1024 ; i < 640*1024+1024 ; ++i)
        b[i] = i & 0xff;

    for (i = 640*1024 ; i < 640*1024+1024 ; ++i)
        "b[<<i>>]=<<b[i]>>\t";
    "\b";
    
    c = new ByteArray(b, 640*1024 + 128, 256);
    for (i = 1 ; i <= c.length() ; ++i)
        "c[<<i>>]=<<c[i]>>\t";
    "\b";


    "---savepoint---\b";
    c = b.subarray(640*1024 + 200, 25);
    for (i = 1 ; i <= c.length() ; ++i)
        "c[<<i>>]=<<c[i]>>\t";
    "\b";
    savepoint();

    c.copyFrom(b, 640*1024+50, 3, 5);
    "after copy:\n";
    for (i = 1 ; i <= c.length() ; ++i)
        "c[<<i>>]=<<c[i]>>\t";
    "\b";

    c.fillValue(77, 10, 5);
    "after fill:\n";
    for (i = 1 ; i <= c.length() ; ++i)
        "c[<<i>>]=<<c[i]>>\t";
    "\b";

    c.packBytes(15, 'A*', 'ABCDE');
    "after packBytes:\n";
    for (i = 1 ; i <= c.length() ; ++i)
        "c[<<i>>]=<<c[i]>>\t";
    "\b";

    "---undo---\b";
    undo();
    for (i = 1 ; i <= c.length() ; ++i)
        "c[<<i>>]=<<c[i]>>\t";
    "\b";
    
    b = new ByteArray(32767);
    b = new ByteArray(32768);
    b = new ByteArray(65536);
    b = new ByteArray(65537);

    cset = new CharacterSet('us-ascii');
    local latin1 = new CharacterSet('latin-1');

    b = new ByteArray(100);
    for (i = 1 ; i <= 26 ; ++i)
        b[i] = i + 64;
    "map to string(ascii): <<b.mapToString(cset)>>\n";
    "map to string(): <<b.mapToString()>>\n";
    "map to string(ascii, 5, 10): <<b.mapToString(cset, 5, 10)>>\n";
    "map to string(nil, 5, 10): <<b.mapToString(nil, 5, 10)>>\n";
    "implicit map to string: <<b>>\n";
    "\b";

    "--- new ByteArray(string) --\b";
    b = new ByteArray('this is a test string: אבגדהו');
    for (i = 1 ; i <= b.length() ; ++i)
        "b[<<i>>]=<<b[i]>>\t";
    "\nimplicit map to string: <<b>>\n";
    "\nmap to string(latin-1, 15): <<b.mapToString(latin1, 15)>>\n";
    "\nmap to string(latin-2, 15): <<b.mapToString('latin-2', 15)>>\n";
    "\nmap to string(nil, 15): <<b.mapToString(nil, 15)>>\n";
    "\b";

    "--- new ByteArray(string, us-ascii) --\b";
    b = new ByteArray('this is a test string: אבגדהו', cset);
    for (i = 1 ; i <= b.length() ; ++i)
        "b[<<i>>]=<<b[i]>>\t";
    "\nimplicit map to string: <<b>>\n";
    "\b";

    "--- new ByteArray(string, latin-2) --\b";
    b = new ByteArray('latin-2 test: \u015e\u0164\u0179',
                      new CharacterSet('iso-8859-2'));
    for (i = 1 ; i <= b.length() ; ++i)
        "b[<<i>>]=0x<<toString(b[i], 16)>>\t";
    "\nimplicit map to string: <<b>>\n";
    "\b";

    "--- String.mapToByteArray(us-ascii) ---\n";
    b = 'hello there'.mapToByteArray(cset);
    for (i = 1 ; i <= b.length() ; ++i)
        "b[<<i>>]=<<b[i]>>\t";
    "\b";

    "--- moving: src=3, dst=5, len=3 ---\b";
    b.copyFrom(b, 3, 5, 3);
    for (i = 1 ; i <= b.length() ; ++i)
        "b[<<i>>]=<<b[i]>>\t";
    "\b";

    "--- moving: src=7, dst=5, len=3 ---\b";
    b.copyFrom(b, 7, 5, 3);
    for (i = 1 ; i <= b.length() ; ++i)
        "b[<<i>>]=<<b[i]>>\t";
    "\b";
}
