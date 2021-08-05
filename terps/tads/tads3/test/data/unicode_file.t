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
    local fpIn, fpOut;

    /* open a file */
    try
    {
        fpIn = File.openTextFile('unicode_text_chars.txt',
                                 FileAccessRead, 'utf-16le');
    }
    catch (FileException fExc)
    {
        "Error opening input file: <<fExc.displayException()>>\n";
        return;
    }

    /* open the output file */
    try
    {
        fpOut = File.openTextFile('unicode_out.txt',
                                  FileAccessWrite, 'utf-16le');
    }
    catch (FileException fExc)
    {
        "Error opening output file: <<fExc.displayException()>>\n";
        return;
    }
    
    /* copy the file */
    for (;;)
    {
        /* read a line */
        local val = fpIn.readFile();
        if (val == nil)
            break;

        /* write it out */
        fpOut.writeFile(val);
    }

    /* close the files */
    fpIn.closeFile();
    fpOut.closeFile();

    /* done */
    "Done!\n";
}

