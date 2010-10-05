#include <tads.h>

main(args)
{
    local fp;
    local buf;
    local siz;

    /* open a file */
    fp = fileOpen('test.bin', 'rr');
    if (fp == nil)
    {
        "Unable to open file\n";
        return;
    }

    /* seek to the end to determine its size */
    fileSeekEnd(fp);
    siz = fileGetPos(fp);
    fileSeek(fp, 0);

    /* create a byte array and read the entire file into the byte array */
    buf = new ByteArray(siz);
    siz = fileRead(fp, buf);
    if (siz != buf.length())
        "Short read: wanted <<buf.length()>> bytes, but only read <<siz>>\b";

    /* show the entire thing */
    for (local addr = 1 ; addr <= buf.length() ; addr += 16)
    {
        local c;
        
        /* show the current address in octal, with a zero base */
        c = toString(addr - 1, 8);
        if (c.length() < 7)
            c = makeString('0', 7 - c.length()) + c;
        "<<c>> ";

        /* show this line */
        for (local i = addr ; i < addr + 16 && i <= buf.length() ; ++i)
        {
            /* get a two-character hex representation of the byte */
            c = toString(buf[i], 16);
            if (c.length() == 1)
                c = '0' + c;

            /* show it */
            "<<c>> ";
        }

        "\n";
    }

    /* done with the file */
    fileClose(fp);

    /* open a file for writing */
    fp = fileOpen('test2.bin', 'wr');
    if (fp == nil)
    {
        "Unable to create output file\n";
        return;
    }

    /* create a byte array */
    buf = new ByteArray(16*256);

    /* fill it up */
    for (local i = 0 ; i < 256 ; ++i)
    {
        /* fill a line of 16 bytes with this byte value */
        for (local j = i*16+1 ; j <= (i+1)*16 ; ++j)
            buf[j] = i;
    }

    /* write it out */
    fileWrite(fp, buf);

    /* done with the file */
    fileClose(fp);
}
