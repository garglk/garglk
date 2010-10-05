#charset "iso-8859-1"

#include <tads.h>
#include <file.h>

main(args)
{
    local f;

    /* create a text file */
    f = File.openTextFile('file1.txt', FileAccessWrite, 'iso-8859-1');

    /* write some data */
    f.writeFile('This is some data for the file.\n'
                + 'Here is the second line of text.\n'
                + 'A blank line follows this one.\n'
                + '\n'
                + 'Okay, here are a few latin-1 characters: “Åéîõü”\n'
                + 'This is the last line!\n');

    /* done - close the file */
    f.closeFile();

    /* now read the file back and display its contents */
    f = File.openTextFile('file1.txt', FileAccessRead, 'iso-8859-1');

    /* read from the file until done */
    for (local linenum = 1 ; ; ++linenum)
    {
        local txt;
        
        /* read another line - stop at eof */
        if ((txt = f.readFile()) == nil)
            break;

        /* show it */
        "<<linenum>>:<<txt>>";
    }

    "\<\<EOF>>\b";
}
