#charset "us-ascii"

#include <tads.h>
#include <file.h>

main(args)
{
    local f;

    /* read a resource file */
    f = File.openTextResource('testres.txt', 'iso-8859-1');

    /* read from the file until done */
    for (local linenum = 1 ; ; ++linenum)
    {
        local txt;
        
        /* read another line - stop at eof */
        if ((txt = f.readFile()) == nil)
            break;

        /* show it */
        "<<linenum>>:<<txt.htmlify()>>";
    }

    "&lt;&lt;EOF>>\b";
}
