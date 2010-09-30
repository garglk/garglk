/* 
 *   Copyright (c) 1987, 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osdosnui.c - DOS/Win-specific functions with no user-interface component
Function
  Provides DOS/Windows implementations of a few miscellaneous osifc
  functions that have no user-interface component.  These are useful for
  linking versions of programs with alternative UI's outside of the set
  implemented in osdos.c and the like.
Notes

Modified
  04/05/02 MJRoberts  - Creation (from osdos.c)
*/

#ifdef __WIN32__
#include <windows.h>
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "std.h"
#include "os.h"

#ifdef TURBO
#include <share.h>
#define _SH_DENYRW SH_DENYRW
#endif

#ifdef DJGPP
#include <fcntl.h>
#include <dpmi.h>
#endif

/* ------------------------------------------------------------------------ */
/*
 *   For djgpp, we must define the _fsopen routine (fopen with share mode),
 *   since this is MS-specific and doesn't appear in the djgpp library.  
 */
#ifdef DJGPP
static FILE *_fsopen(const char *fname, const char *mode, int sh_flags)
{
    int old_flags;
    FILE *fp;

    /* set the new share flags, remembering the previous flags */
    old_flags = __djgpp_share_flags;
    __djgpp_share_flags = sh_flags;

    /* open the file */
    fp = fopen(fname, mode);

    /* restore the old share flags */
    __djgpp_share_flags = old_flags;

    /* return the result of the open */
    return fp;
}
#endif /* DJGPP */



/* ------------------------------------------------------------------------ */
/*
 *   File I/O 
 */

/*
 *   Open text file for reading and writing, keeping existing data, creating
 *   a new file if the file doesn't already exist.  
 */
osfildef *osfoprwt(const char *fname, os_filetype_t typ)
{
    osfildef *fp;

    VARUSED(typ);

    /* 
     *   try opening in "r+" mode - this will open an existing file in
     *   read/write mode, but will fail if the file doesn't already exist 
     */
    fp = _fsopen(fname, "r+", _SH_DENYRW);

    /* if that succeeded, return the file */
    if (fp != 0)
        return fp;

    /* 
     *   the file doesn't exist - create a new file in "w+" mode, which
     *   creates a new file and opens it in read/write mode 
     */
    return _fsopen(fname, "w+", _SH_DENYRW);
}

/*
 *   Open binary file for reading and writing, keeping existing data,
 *   creating a new file if the file doesn't already exist.  
 */
osfildef *osfoprwb(const char *fname, os_filetype_t typ)
{
    osfildef *fp;

    VARUSED(typ);

    /* 
     *   try opening in "r+b" mode - this will open an existing file in
     *   read/write mode, but will fail if the file doesn't already exist 
     */
    fp = _fsopen(fname, "r+b", _SH_DENYRW);

    /* if that succeeded, return the file */
    if (fp != 0)
        return fp;

    /* 
     *   the file doesn't exist - create a new file in "w+b" mode, which
     *   creates a new file and opens it in read/write mode 
     */
    return _fsopen(fname, "w+b", _SH_DENYRW);
}

#ifdef __WIN32__
/* ------------------------------------------------------------------------ */
/*
 *   Get the system clock in milliseconds.  This is extremely easy for Win32
 *   console apps, since there's an API that does exactly what we want.  
 */
long os_get_sys_clock_ms(void)
{
    return (long)GetTickCount();
}

#else /* __WIN32__ */

/* ------------------------------------------------------------------------ */
/*
 *   Calculate the current system time in milliseconds.  Returns a value
 *   relative to an arbitrary base time.  
 */
long os_get_sys_clock_ms(void)
{
    static unsigned long last_ticks;
    long curtime;

    /* get the current system clock reading in clock ticks */
#ifdef DJGPP
    __dpmi_regs regs;

    regs.h.ah = 0;
    __dpmi_int(0x1a, &regs);
    curtime = (regs.x.cx << 16) | regs.x.dx;
#else /* DJGPP */
    asm {
        push   bp
        mov    ah, 00h
        int    1ah
        pop    bp

        mov    word ptr [curtime], dx
        mov    word ptr [curtime+2], cx
    }
#endif /* DJGPP */

    /* 
     *   Check for clock rollover.  The clock resets to 0 ticks at midnight.
     *   If the new tick count is lower than the last tick count we read, add
     *   1573040 (the number of ticks in 24 hours) until it's greater.  This
     *   ensures that we have continuity between readings.  
     */
    while (curtime < last_ticks)
        curtime += 1573040;

    /* save the last tick count for checking rollover next time through */
    last_ticks = curtime;

    /* 
     *   Convert the tick count to milliseconds - there are 18.2065 ticks
     *   per second, hence 18.2065 ticks per 1000 milliseconds, hence
     *   1000/18.2065 milliseconds per tick.  This is approximately 55
     *   ticks per millisecond.  Return the result of the conversion.  
     */
    return curtime * 55;
}

#endif /* __WIN32__ */

