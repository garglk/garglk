/* Copyright 2001, 2002 Michael J. Roberts */
/*
 *   gameinfo.t - game information utility for tads 2
 *   
 *   This header file is designed to be #include'd in a TADS game's source
 *   file to define a simple utility function that generates a
 *   "GameInfo.txt" file during preinit.  IMPORTANT: Include this file
 *   *before* std.t if you also include std.t.
 *   
 *   The GameInfo.txt file provides "card catalog" information about the
 *   game.  Automated tools, such as archive management software, can
 *   extract the information for applications such as searching and
 *   browsing.
 *   
 *   To include game information in your game, you must do the following:
 *   
 *   1.  Add a #include directive to your game's source code to include
 *   this file.
 *   
 *      #include <gameinfo.t>
 *   
 *   IMPORTANT: The order of inclusion is important.  If you're including
 *   adv.t and std.t in addition to this file, you must use the following
 *   order:
 *   
 *   #include <adv.t>
 *.  #include <gameinfo.t>
 *.  #include <std.t>
 *   
 *   2.  Define a function in your game called getGameInfo().  This
 *   function should simply return a list value.  The list should consist
 *   of pairs of strings; the first string in each pair is the name of a
 *   parameter, and the second of each pair is the value for the parameter.
 *   For example:
 *   
 *.      getGameInfo: function
 *.      {
 *.        return ['Name', 'My Game',
 *.                'Byline', 'by R. Teeterwaller',
 *.                'Desc', 'A simple test game.];
 *.      }
 *   
 *   3.  If you're using the standard preinit() function from std.t, you
 *   can skip this step.  Otherwise, add this line to your own preinit()
 *   function, anywhere within the function:
 *   
 *      writeGameInfo(getGameInfo()); 
 *   
 *   4.  After you compile your game, use the tads resource tool to bundle
 *   GameInfo.txt into your .gam file, just as though it were an HTML TADS
 *   multimedia resource.  If you are including other multimedia resources
 *   in your game, simply add GameInfo.txt to the list of files you bundle.
 *   If you don't have any other resources, add this command line to your
 *   build script, immediately after the TADS compiler commnd line:
 *   
 *.     tadsrsc mygame.gam -add GameInfo.txt
 *   
 *   (If you're using the 32-bit Windows version, the command name is
 *   tadsrsc32.  The command name may vary on other platforms.)  
 */

#pragma C+

/* indicate that we've included this file, for std.t's use */
#define GAMEINFO_INCLUDED

/*
 *   write the name/value pairs from the given list to the file
 *   'GameInfo.txt' 
 */
writeGameInfo: function(info)
{
    local fp;
    local i;
    local cnt;
    
    /* open the output file */
    fp = fopen('GameInfo.txt', 'wt');

    /* write the values */
    for (i = 1, cnt = length(info) ; i <= cnt ; i += 2)
    {
        /* write the name, a colon, and the value */
        fwrite(fp, info[i]);
        fwrite(fp, ': ');
        fwrite(fp, info[i+1]);
        fwrite(fp, '\n');
    }

    /* done with the file */
    fclose(fp);
}

/*
 *   Utility function to retrieve today's date in format YYY-MM-DD.
 */
getGameInfoToday: function
{
    local dt;
    local mm, dd;

    /* 
     *   get the current time/date information - if this is called during
     *   preinit, it will give the date on which the game was compiled,
     *   which can be used as a simple way of keeping the release date
     *   field in the game information automatically updated 
     */
    dt = gettime(GETTIME_DATE_AND_TIME);

    /* 
     *   get the month, and make sure it's at least two digits, adding a
     *   leading zero if necessary 
     */
    mm = cvtstr(dt[2]);
    if (length(mm) == 1)
        mm = '0' + mm;

    /* get the day of the month, and make sure it's at least two digits */
    dd = cvtstr(dt[3]);
    if (length(dd) == 1)
        dd = '0' + dd;

    /* build the return string in YYYY-MM-DD format */
    return cvtstr(dt[1]) + '-' + mm + '-' + dd;
}
