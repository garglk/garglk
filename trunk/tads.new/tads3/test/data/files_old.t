/*
 *   files.t - test of file I/O operations 
 */

#include "tads.h"
#include "t3.h"


_say_embed(str) { tadsSay(str); }

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime Error: <<errno_>>"
    errno_ = 0
;

_main(args)
{
    try
    {
        t3SetSay(_say_embed);
        main();
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

main()
{
    local fp;
    local i;

    /* -------------------------------------------------------------------- */
    /*
     *   text file
     */

    /* open a file */
    fp = fileOpen('test.txt', 'wt', 'us-ascii');
    if (fp == nil)
    {
        "Error opening file test.txt for writing\n";
        return;
    }

    /* write some data */
    for (i = 0 ; i < 100 ; ++i)
        fileWrite(fp, 'This is line ' + i + '!!!\n');
    fileWrite(fp, 'Some extended characters: '
           + '\u2039 \u2122 \u00A9 \u00AE \u203A\n');

    /* close the file */
    fileClose(fp);

    /* open the file for reading */
    fp = fileOpen('test.txt', 'rt', 'us-ascii');
    if (fp == nil)
    {
        "Error opening file test.txt for reading\n";
        return;
    }

    /* read the data */
    for (i = 0 ; ; ++i)
    {
        local val;

        val = fileRead(fp);
        if (val == nil)
            break;
        "<<i>>: <<val>>\n";
    }

    fileClose(fp);

    /* -------------------------------------------------------------------- */
    /*
     *   binary file
     */

    fp = fileOpen('test.bin', 'wb');
    if (fp == nil)
    {
        "Error opening file test.bin for writing\n";
        return;
    }

    /* write some data */
    for (i = 0 ; i <= 100 ; i += 20)
    {
        fileWrite(fp, i);
        fileWrite(fp, 'String ' + i);
    }

    fileClose(fp);

    /* open it for reading */
    fp = fileOpen('test.bin', 'rb');
    if (fp == nil)
    {
        "Error opening file test.bin for reading\n";
        return;
    }

    /* read the data back */
    for (i = 0 ; ; ++i)
    {
        local ival, sval;
        
        if ((ival = fileRead(fp)) == nil || (sval = fileRead(fp)) == nil)
            break;

        "<<i>>: type <<dataType(ival)>>, value '<<ival>>';
        type <<dataType(sval)>>, value '<<sval>>'\n";
    }

    fileClose(fp);

    /* open a file with a weird name */
    local fname = 'test\u00e4\u00eb\u00ef\u00f6\u00fc\u00ff.dat';
    "Opening <<fname>> (that's 'test' + 'aeiouy' with umlauts + '.dat')\n";
    fp = fileOpen(fname, 'wt', 'cp437');
    "fp = << fp == nil ? 'nil' : fp >>\n";
    fileWrite(fp, 'Hello there!\n');
    fileWrite(fp, 'Filename = ' + fname + '\n');
    fileClose(fp);

    "Done!\n";
}

