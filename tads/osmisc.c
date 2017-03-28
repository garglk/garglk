/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

/* osmisc.c -- misc leftovers */

#include <string.h>
#include <time.h>

#include <unistd.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>	/* for Sleep() */
#endif

#include "os.h"

/*
 *   Generate a filename for a character-set mapping file.  This function
 *   should determine the current native character set in use, if
 *   possible, then generate a filename, according to system-specific
 *   conventions, that we should attempt to load to get a mapping between
 *   the current native character set and the internal character set
 *   identified by 'internal_id'.
 *   
 *   The internal character set ID is a string of up to 4 characters.
 *   
 *   On DOS, the native character set is a DOS code page.  DOS code pages
 *   are identified by 3- or 4-digit identifiers; for example, code page
 *   437 is the default US ASCII DOS code page.  We generate the
 *   character-set mapping filename by appending the internal character
 *   set identifier to the DOS code page number, then appending ".TCP" to
 *   the result.  So, to map between ISO Latin-1 (internal ID = "La1") and
 *   DOS code page 437, we would generate the filename "437La1.TCP".
 *   
 *   Note that this function should do only two things.  First, determine
 *   the current native character set that's in use.  Second, generate a
 *   filename based on the current native code page and the internal ID.
 *   This function is NOT responsible for figuring out the mapping or
 *   anything like that -- it's simply where we generate the correct
 *   filename based on local convention.
 *   
 *   'filename' is a buffer of at least OSFNMAX characters.
 *   
 *   'argv0' is the executable filename from the original command line.
 *   This parameter is provided so that the system code can look for
 *   mapping files in the original TADS executables directory, if desired.
 */
void os_gen_charmap_filename(char *filename, char *internal_id, char *argv0)
{
    filename[0] = 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   get a suitable seed for a random number generator; should use the
 *   system clock or some other source of an unpredictable and changing
 *   seed value 
 */
void os_rand(long *seed)
{
    time_t t;
    time( &t );
    *seed = (long)t;
}

/*
 *   Xorshift PRNG from http://www.jstatsoft.org/v08/i14/paper
 */
static uint32_t seed[4];

static unsigned long xorshift(void)
{
    uint32_t x;
    x = seed[0] ^ (seed[0] << 11);
    seed[0] = seed[1];
    seed[1] = seed[2];
    seed[2] = seed[3];

    return (seed[3] = (seed[3] ^ (seed[3] >> 19)) ^ (x ^ (x >> 8)));
}

static void xorinit(void)
{
    seed[0] = time(NULL);
    seed[1] = seed[0] << 1;
    seed[2] = getpid();
    seed[3] = seed[1] & ~seed[2];
    xorshift();
}

/*
 *   Generate random bytes for use in seeding a PRNG.
 */
void os_gen_rand_bytes(unsigned char *buf, size_t len)
{
    /* seed the Xorshift PRNG */
    xorinit();

    int ct, val;
    for (ct = 0; ct < len; ct++)
    {
        val = xorshift();
        buf[ct] = (val - 1) & 0xFF;
    }
}


/* flush any buffered display output */
void os_flush(void)
{
}

/*
 *   Update the display - process any pending drawing immediately.  This
 *   only needs to be implemented for operating systems that use
 *   event-driven drawing based on window invalidations; the Windows and
 *   Macintosh GUI's both use this method for drawing window contents.
 *   
 *   The purpose of this routine is to refresh the display prior to a
 *   potentially long-running computation, to avoid the appearance that the
 *   application is frozen during the computation delay.
 *   
 *   Platforms that don't need to process events in the main thread in order
 *   to draw their window contents do not need to do anything here.  In
 *   particular, text-mode implementations generally don't need to implement
 *   this routine.
 *   
 *   This routine doesn't absolutely need a non-empty implementation on any
 *   platform, but it will provide better visual feedback if implemented for
 *   those platforms that do use event-driven drawing.  
 */
void os_update_display()
{
}

/* ------------------------------------------------------------------------ */
/* 
 *   os_plain() - Use plain ascii mode for the display.  If possible and
 *   necessary, turn off any text formatting effects, such as cursor
 *   positioning, highlighting, or coloring.  If this routine is called,
 *   the terminal should be treated as a simple text stream; users might
 *   wish to use this mode for things like text-to-speech converters.
 *   
 *   Purely graphical implementations that cannot offer a textual mode
 *   (such as Mac OS or Windows) can ignore this setting.
 *   
 *   If this routine is to be called, it must be called BEFORE os_init().
 *   The implementation should set a flag so that os_init() will know to
 *   set up the terminal for plain text output.  
 */
/* 
 *   some platforms (e.g. Mac OS) define this to be a null macro, so don't
 *   define a prototype in those cases 
 */
void os_plain(void)
{
}
/*
 *   Set non-stop mode.  This tells the OS layer that it should disable any
 *   MORE prompting it would normally do.
 *   
 *   This routine is needed only when the OS layer handles MORE prompting; on
 *   character-mode platforms, where the prompting is handled in the portable
 *   console layer, this can be a dummy implementation.  
 */
void os_nonstop_mode(int flag)
{
}

/*
 *   Sleep for a while.  This should simply pause for the given number of
 *   milliseconds, then return.  On multi-tasking systems, this should use
 *   a system API to unschedule the current process for the desired delay;
 *   on single-tasking systems, this can simply sit in a wait loop until
 *   the desired interval has elapsed.  
 */
void os_sleep_ms(long delay_in_milliseconds)
{
#ifdef _WIN32
    Sleep(delay_in_milliseconds);
#else
    usleep(delay_in_milliseconds * 1000);
#endif
}

/* 
 *   Pause prior to exit, if desired.  This is meant to be called by
 *   portable code just before the program is to be terminated; it can be
 *   implemented to show a prompt and wait for user acknowledgment before
 *   proceeding.  This is useful for implementations that are using
 *   something like a character-mode terminal window running on a graphical
 *   operating system: this gives the implementation a chance to pause
 *   before exiting, so that the window doesn't just disappear
 *   unceremoniously.
 *   
 *   This is allowed to do nothing at all.  For regular character-mode
 *   systems, this routine usually doesn't do anything, because when the
 *   program exits, the terminal will simply return to the OS command
 *   prompt; none of the text displayed just before the program exited will
 *   be lost, so there's no need for any interactive pause.  Likewise, for
 *   graphical systems where the window will remain open, even after the
 *   program exits, until the user explicitly closes the window, there's no
 *   need to do anything here.
 *   
 *   If this is implemented to pause, then this routine MUST show some kind
 *   of prompt to let the user know we're waiting.  In the simple case of a
 *   text-mode terminal window on a graphical OS, this should simply print
 *   out some prompt text ("Press a key to exit...") and then wait for the
 *   user to acknowledge the prompt (by pressing a key, for example).  For
 *   graphical systems, the prompt could be placed in the window's title
 *   bar, or status-bar, or wherever is appropriate for the OS.  
 */
void os_expause(void)
{
}

/* 
 *   Install/uninstall the break handler.  If possible, the OS code should
 *   set (if 'install' is true) or clear (if 'install' is false) a signal
 *   handler for keyboard break signals (control-C, etc, depending on
 *   local convention).  The OS code should set its own handler routine,
 *   which should note that a break occurred with an internal flag; the
 *   portable code uses os_break() from time to time to poll this flag.  
 */
void os_instbrk(int install)
{
}

/*
 *   Set the default saved-game extension.  This routine will NOT be
 *   called when we're using the standard saved game extension; this
 *   routine will be invoked only if we're running as a stand-alone game,
 *   and the game author specified a non-standard saved-game extension
 *   when creating the stand-alone game.
 *   
 *   This routine is not required if the system does not use the standard,
 *   semi-portable os0.c implementation.  Even if the system uses the
 *   standard os0.c implementation, it can provide an empty routine here
 *   if the system code doesn't need to do anything special with this
 *   information.
 *   
 *   The extension is specified as a null-terminated string.  The
 *   extension does NOT include the leading period.  
 */
void os_set_save_ext(const char *ext)
{
}

/*
 *   Generate the name of the character set mapping table for Unicode
 *   characters to and from the given local character set.  Fills in the
 *   buffer with the implementation-dependent name of the desired
 *   character set map.  See below for the character set ID codes.
 *   
 *   For example, on Windows, the implementation would obtain the
 *   appropriate active code page (which is simply a Windows character set
 *   identifier number) from the operating system, and build the name of
 *   the Unicode mapping file for that code page, such as "CP1252".  On
 *   Macintosh, the implementation would look up the current script system
 *   and return the name of the Unicode mapping for that script system,
 *   such as "ROMAN" or "CENTEURO".
 *   
 *   If it is not possible to determine the specific character set that is
 *   in use, this function should return "asc7dflt" (ASCII 7-bit default)
 *   as the character set identifier on an ASCII system, or an appropriate
 *   base character set name on a non-ASCII system.  "asc7dflt" is the
 *   generic character set mapping for plain ASCII characters.
 *   
 *   The given buffer must be at least 32 bytes long; the implementation
 *   must limit the result it stores to 32 bytes.  (We use a fixed-size
 *   buffer in this interface for simplicity, and because there seems no
 *   need for greater flexibility in the interface; a character set name
 *   doesn't carry very much information so shouldn't need to be very
 *   long.  Note that this function doesn't generate a filename, but
 *   simply a mapping name; in practice, a map name will be used to
 *   construct a mapping file name.)
 *   
 *   Because this function obtains the Unicode mapping name, there is no
 *   need to specify the internal character set to be used: the internal
 *   character set is Unicode.  
 */
/*
 *   Implementation note: when porting this routine, the convention that
 *   you use to name your mapping files is up to you.  You should simply
 *   choose a convention for this implementation, and then use the same
 *   convention for packaging the mapping files for your OS release.  In
 *   most cases, the best convention is to use the names that the Unicode
 *   consortium uses in their published cross-mapping listings, since
 *   these listings can be used as the basis of the mapping files that you
 *   include with your release.  For example, on Windows, the convention
 *   is to use the code page number to construct the map name, as in
 *   CP1252 or CP1250.  
 */
void os_get_charmap(char *mapname, int charmap_id)
{
#ifndef GLK_UNICODE
    strcpy(mapname, "asc7dflt");
#else
    switch (charmap_id)
    {
        case OS_CHARMAP_DISPLAY:
        case OS_CHARMAP_FILECONTENTS:
            strcpy(mapname, "utf-8");
            break;

        case OS_CHARMAP_FILENAME:
        default:
            strcpy(mapname, "asc7dflt");
            break;
    }
#endif
}

int os_break(void)
{
  return 0;
}

/*
 *   Implementation of vasprintf for MinGW.
 *   NOTE: Because vasprintf is not standard, unconditionally define it
 *   here, but with a name that won't clash with versions that do exist.
 */
static int xvasprintf(char **sptr, const char *fmt, va_list argv)
{
    va_list argv_copy;
    int wanted;

    va_copy(argv_copy, argv);
    wanted = vsnprintf(*sptr = NULL, 0, fmt, argv);
    if((wanted < 0) || ((*sptr = malloc( 1 + wanted )) == NULL))
        return -1;

    return vsprintf(*sptr, fmt, argv_copy);
}

int os_asprintf(char **bufptr, const char *fmt, ...)
{
    int retval;
    va_list argv;
    va_start(argv, fmt);
    retval = xvasprintf(bufptr, fmt, argv);
    va_end(argv);
    return retval;
}

int os_vasprintf(char **bufptr, const char *fmt, va_list ap)
{
    return xvasprintf(bufptr, fmt, ap);
}
