#charset "us-ascii"

/* 
 *   Copyright (c) 2001, 2006 Michael J. Roberts.
 *   
 *   Permission is granted to anyone to use this file, including modified
 *   and derived versions, without charge, provided this original copyright
 *   notice is retained.  
 *   
 *   This is an add-in utility for tads 3 games to make it simpler to add
 *   game information to a compiled game.  
 *   
 *   The game information mechanism lets you bind certain documentary
 *   information about the game directly into the compiled (.t3) file.  The
 *   information can then be extracted by automated tools; for example,
 *   archive maintainers can extract the game information and use it to
 *   describe the archive entry.  
 */

/*   
 *   To use this module, a game must simply call our function
 *   writeGameInfo(), passing in a lookup table and a destination filename.
 *   
 *   The lookup table consists of one entry per game information item.  For
 *   each entry, the key is the name of the item, and the associated value
 *   is a string giving the text of the item.
 *   
 *   The destination filename should usually be "GameInfo.txt", since
 *   that's the name of the game information resource that must be bound
 *   into the .t3 file.  However, the name isn't actually important until
 *   the resource compiler (t3res) is invoked, at which point an arbitrary
 *   filename can be mapped to the required resource name if desired.  Note
 *   that we'll overwrite any existing file with the given name.
 *   
 *   writeGameInfo() will throw a FileException if an error occurs writing
 *   the data to the file.
 *   
 *   writeGameInfo() should be called during pre-initialization so that the
 *   game information is generated immediately after compilation is
 *   finished.  Here's an example of how you might do this:
 *   
 *   PreinitObject
 *.    execute()
 *.    {
 *.      local tab = new LookupTable();
 *.  
 *.      tab['Name'] = 'My Test Game';
 *.      tab['Author'] = 'Bob I. Fiction';
 *.      tab['Desc'] = 'My simple test game, just to demonstrate how
 *.                    to write game information.';
 *.  
 *.      writeGameInfo(tab, 'GameInfo.txt');
 *.    }
 *.  ;
 *   
 *   After pre-initialization finishes, you must finish the job by running
 *   the resource compiler, with a command line something like this:
 *   
 *      t3res mygame.t3 -add GameInfo.txt
 *   
 *   If you didn't call the output file GameInfo.txt, you can map the
 *   filename to the proper resource name with a command like this:
 *   
 *     t3res mygame.t3 -add outfile.xyz=GameInfo.txt 
 */

#include <tads.h>
#include <file.h>

/* TADS GameInfo writer */
gameInfoWriter: object
    /* 
     *   Write the game information from the given LookupTable to the given
     *   file.  Each key/value pair in the LookupTable gives the GameInfo
     *   key and the corresponding value string for that key.  
     */
    writeGameInfo(tab, fname)
    {
        local f;
        
        /* 
         *   open the file - note that the GameInfo.txt resource is
         *   required to be encoded in UTF-8, so open the file with the
         *   UTF-8 character set 
         */
        f = File.openTextFile(fname, FileAccessWrite, 'utf-8');

        /* write each entry in the table */
        tab.forEachAssoc({key, val: f.writeFile(key + ': ' + val + '\n')});
        
        /* done with the file - close it */
        f.closeFile();
    }

    /*
     *   Get today's date as a string in the format YYYY-MM-DD.  This can
     *   be used as a simple way of keeping the release date in the game
     *   information up to date with the latest compilation.  
     */
    getGameInfoToday()
    {
        local dt;
        local mm, dd;
        
        /* get the current date */
        dt = getTime(GetTimeDateAndTime);
        
        /* get the month, and add a leading zero if it's only one digit */
        mm = (dt[2] < 10 ? '0' : '') + dt[2];
        
        /* get the day, and add a leading zero if it's only one digit */
        dd = (dt[3] < 10 ? '0' : '') + dt[3];
        
        /* build and return the full YYYY-MM-DD date string */
        return toString(dt[1]) + '-' + mm + '-' + dd;
    }
;

