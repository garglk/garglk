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
    savepoint();

    c = b.subarray(640*1024 + 200, 25);
    for (i = 1 ; i <= c.length() ; ++i)
        "c[<<i>>]=<<c[i]>>\t";
    "\b";

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

    b = new ByteArray(100);
    for (i = 1 ; i <= 26 ; ++i)
        b[i] = i + 64;
    "map to string: <<b.mapToString(cset)>>\n";
    "\b";

    b = 'hello there'.mapToByteArray(cset);
    "map to byte array:\n";
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
