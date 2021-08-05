/*
 *   files.t - test of file I/O operations 
 */

#include "tads.h"
#include "t3.h"
#include "bytearr.h"
#include "file.h"
#include "bignum.h"


main(args)
{
    local fp;
    local i;

    /* -------------------------------------------------------------------- */
    /*
     *   text file 
     */

    /* open a file */
    try
    {
        fp = File.openTextFile('test.txt', FileAccessWrite, 'asc7dflt');
    }
    catch (FileException fExc)
    {
        "Error opening file test.txt for writing:
         <<fExc.displayException()>>\n";
        return;
    }

    /* write some data */
    for (i = 0 ; i < 100 ; ++i)
        fp.writeFile('This is line ' + i + '!!!\n');
    fp.writeFile('Some extended characters: '
                 + '\u2039 \u2122 \u00A9 \u00AE \u203A\n');

    /* close the file */
    fp.closeFile();

    /* open the file for reading */
    try
    {
        fp = File.openTextFile('test.txt', FileAccessRead, 'asc7dflt');
    }
    catch (FileException fExc)
    {
        "Error opening file test.txt for reading:
        <<fExc.displayException()>>\n";
        return;
    }

    "test.txt: size = <<fp.getFileSize()>>\n";

    /* read the data */
    for (i = 0 ; ; ++i)
    {
        local val;

        val = fp.readFile();
        if (val == nil)
            break;
        "<<i>>: <<val>>\n";
    }

    fp.closeFile();

    /* -------------------------------------------------------------------- */
    /*
     *   text file read/write with seeking 
     */
    try
    {
        fp = File.openTextFile(
            'test.txt', FileAccessReadWriteKeep, 'us-ascii');
    }
    catch (FileException fExc)
    {
        "Error opening file test.txt for read/write/keep:
        <<fExc.displayException()>>\n";
        return;
    }

    "test.txt: size = <<fp.getFileSize()>>\n";

    /* read back the data */
    local line50 = nil;
    for (i = 0 ; ; ++i)
    {
        local sol = fp.getPos();
        local val = fp.readFile();
        if (val == nil)
            break;
        else if (val.find('line 50') != nil)
            line50 = sol;
    }

    /* seek back to line 50 */
    "Seeking to <<line50>>\n";
    fp.setPos(line50);

    /* re-write the remainder of the file */
    for (i = 50 ; i < 75 ; ++i)
        fp.writeFile('This is the NEW line <<i>>!!!\n');

    /* done */
    fp.closeFile();

    /* -------------------------------------------------------------------- */
    /*
     *   binary file 
     */

    try
    {
        fp = File.openDataFile('test.bin', FileAccessWrite);
    }
    catch (FileException fExc)
    {
        "Error opening file test.bin for writing\n";
        return;
    }

    /* write some data */
    for (i = 0 ; i <= 100 ; i += 20)
    {
        fp.writeFile(i);
        fp.writeFile('String ' + i);
    }

    /* write a couple of BigNumber values */
    fp.writeFile(1.2345);
    fp.writeFile('BigNumber 1.2345');

    fp.writeFile(new BigNumber(12345, 10).logE());
    fp.writeFile('BigNumber ln(12345)');

    /* write a byte array */
    {
        local arr = new ByteArray(20);

        for (local i = 1 ; i <= 20 ; ++i)
            arr[i] = i*5;

        fp.writeFile(arr);
        fp.writeFile('ByteArray(20)');
    }

    /* done with the file for this round */
    fp.closeFile();

    /* open it for reading */
    try
    {
        fp = File.openDataFile('test.bin', FileAccessRead);
    }
    catch (FileException fExc)
    {
        "Error opening file test.bin for reading\n";
        return;
    }

    /* read the data back */
    for (i = 0 ; ; ++i)
    {
        local ival, sval;

        /* read the pair of values */
        if ((ival = fp.readFile()) == nil || (sval = fp.readFile()) == nil)
            break;

        /* show the first value in the pair */
        if (dataType(ival) == TypeObject && ival.ofKind(ByteArray))
        {
            "<<i>>: type ByteArray, value: [";
            for (local j = 1 ; j <= ival.length() ; ++j)
            {
                if (j > 1) ", ";
                "<<ival[j]>>";
            }
            "]";
        }
        else
        {
            "<<i>>: type <<dataType(ival)>>, value '<<ival>>'";
        }

        /* show the second of the pair (always a string) */
        "; type <<dataType(sval)>>, value '<<sval>>'\n";
    }

    /* done with the file */    
    fp.closeFile();

    /* -------------------------------------------------------------------- */
    /* 
     *   open a file with a weird name 
     */

    local fname = 'test\u00e4\u00eb\u00ef\u00f6\u00fc\u00ff.dat';
    "Opening <<fname>> (that's 'test' + 'aeiouy' with umlauts + '.dat')\n";
    fp = File.openTextFile(fname, FileAccessWrite, 'cp437');
    fp.writeFile('Hello there!\n');
    fp.writeFile('Filename = ' + fname + '\n');
    fp.closeFile();


    /* -------------------------------------------------------------------- */
    /*
     *   raw file 
     */

    /* open a raw file for writing */
    fp = File.openRawFile('test.raw', FileAccessWrite);

    /* write some bytes */
    local arr = new ByteArray(100);
    for (local i = 1 ; i <= arr.length() ; ++i)
        arr[i] = i;

    fp.writeBytes(arr, 11, 10);
    fp.writeBytes(arr, 1,  10);
    fp.writeBytes(arr, 21, 10);
    fp.writeBytes(arr, 31);

    /* done with the file */
    fp.closeFile();

    /* -------------------------------------------------------------------- */
    /*
     *   done 
     */
    "Done!\n";
}

