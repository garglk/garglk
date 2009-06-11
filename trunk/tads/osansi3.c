/* osansi3.c -- misc leftovers */

#include "os.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>	/* for Sleep() */
#endif

#include <time.h>
#include <ctype.h>
#include <unistd.h>

/* Convert a string to all-lowercase */
char *os_strlwr(char *s)
{
    char *sptr;

    sptr = s;
    while (*sptr != 0) {
        *sptr = tolower((unsigned char)*sptr);
        sptr++;
    }
    return s;
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


/* ------------------------------------------------------------------------ */
/*
 *   Display routines.
 *   
 *   Our display model is a simple stdio-style character stream.
 *   
 *   In addition, we provide an optional "status line," which is a
 *   non-scrolling area where a line of text can be displayed.  If the status
 *   line is supported, text should only be displayed in this area when
 *   os_status() is used to enter status-line mode (mode 1); while in status
 *   line mode, text is written to the status line area, otherwise (mode 0)
 *   it's written to the normal main text area.  The status line is normally
 *   shown in a different color to set it off from the rest of the text.
 *   
 *   The OS layer can provide its own formatting (word wrapping in
 *   particular) if it wants, in which case it should also provide pagination
 *   using os_more_prompt().  
 */

/* redraw the screen */
void os_redraw(void)
{
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
#ifndef os_plain
/* 
 *   some platforms (e.g. Mac OS) define this to be a null macro, so don't
 *   define a prototype in those cases 
 */
void os_plain(void)
{
}
#endif


/*
 *   Enter HTML mode.  This is only used when the run-time is compiled
 *   with the USE_HTML flag defined.  This call instructs the renderer
 *   that HTML sequences should be parsed; until this call is made, the
 *   renderer should not interpret output as HTML.  Non-HTML
 *   implementations do not need to define this routine, since the
 *   run-time will not call it if USE_HTML is not defined.  
 */
void os_start_html(void)
{
}

/* exit HTML mode */
void os_end_html(void)
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
 *   Update progress display with current info, if appropriate.  This can
 *   be used to provide a status display during compilation.  Most
 *   command-line implementations will just ignore this notification; this
 *   can be used for GUI compiler implementations to provide regular
 *   display updates during compilation to show the progress so far.  
 */
void os_progress(const char *fname, unsigned long linenum)
{
}

/* 
 *   Set busy cursor.  If 'flag' is true, provide a visual representation
 *   that the system or application is busy doing work.  If 'flag' is
 *   false, remove any visual "busy" indication and show normal status.
 *   
 *   We provide a prototype here if your osxxx.h header file does not
 *   #define a macro for os_csr_busy.  On many systems, this function has
 *   no effect at all, so the osxxx.h header file simply #define's it to
 *   do an empty macro.  
 */
#ifndef os_csr_busy
void os_csr_busy(int flag)
{
}
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Get the current system high-precision timer.  This function returns a
 *   value giving the wall-clock ("real") time in milliseconds, relative
 *   to any arbitrary zero point.  It doesn't matter what this value is
 *   relative to -- the only important thing is that the values returned
 *   by two different calls should differ by the number of actual
 *   milliseconds that have elapsed between the two calls.  On most
 *   single-user systems, for example, this will probably return the
 *   number of milliseconds since the user turned on the computer.
 *   
 *   True millisecond precision is NOT required.  Each implementation
 *   should simply use the best precision available on the system.  If
 *   your system doesn't have any kind of high-precision clock, you can
 *   simply use the time() function and multiply the result by 1000 (but
 *   see the note below about exceeding 32-bit precision).
 *   
 *   However, it *is* required that the return value be in *units* of
 *   milliseconds, even if your system clock doesn't have that much
 *   precision; so on a system that uses its own internal clock units,
 *   this routine must multiply the clock units by the appropriate factor
 *   to yield milliseconds for the return value.
 *   
 *   It is also required that the values returned by this function be
 *   monotonically increasing.  In other words, each subsequent call must
 *   return a value that is equal to or greater than the value returned
 *   from the last call.  On some systems, you must be careful of two
 *   special situations.
 *   
 *   First, the system clock may "roll over" to zero at some point; for
 *   example, on some systems, the internal clock is reset to zero at
 *   midnight every night.  If this happens, you should make sure that you
 *   apply a bias after a roll-over to make sure that the value returned
 *   from this return continues to increase despite the reset of the
 *   system clock.
 *   
 *   Second, a 32-bit signed number can only hold about twenty-three days
 *   worth of milliseconds.  While it seems unlikely that a TADS game
 *   would run for 23 days without a break, it's certainly reasonable to
 *   expect that the computer itself may run this long without being
 *   rebooted.  So, if your system uses some large type (a 64-bit number,
 *   for example) for its high-precision timer, you may want to store a
 *   zero point the very first time this function is called, and then
 *   always subtract this zero point from the large value returned by the
 *   system clock.  If you're using time(0)*1000, you should use this
 *   technique, since the result of time(0)*1000 will almost certainly not
 *   fit in 32 bits in most cases.
 */
long os_get_sys_clock_ms(void)
{
	static time_t time_zero;
	if (time_zero == 0)
		time_zero = time(0);
	return ((time(0) - time_zero) * 1000);
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

#if 0
	/*
	 *   calculate the time when we'll be done by adding the delay to the
	 *   current time
	 */
	done_time = os_get_sys_clock_ms() + delay_in_milliseconds;

	/* loop until the system clock says we're done */
	while (os_get_sys_clock_ms() < done_time)
		/* do nothing but soak up CPU cycles... */;
#endif
}

/* set a file's type information */
void os_settype(const char *f, os_filetype_t typ)
{
}

/*
 *   get filename from startup parameter, if possible; returns true and
 *   fills in the buffer with the parameter filename on success, false if
 *   no parameter file could be found 
 */
int os_paramfile(char *buf)
{
	return FALSE;
}

/*
 *   Uninitialize.  This is called prior to progam termination to reverse
 *   the effect of any changes made in os_init().  For example, if
 *   os_init() put the terminal in raw mode, this should restore the
 *   previous terminal mode.  This routine should not terminate the
 *   program (so don't call exit() here) - the caller might have more
 *   processing to perform after this routine returns.
 */
void os_uninit(void)
{
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
 *   Check for user break ("control-C", etc) - returns true if a break is
 *   pending, false if not.  If this returns true, it should "consume" the
 *   pending break (probably by simply clearing the OS code's internal
 *   break-pending flag).  
 */
int os_break(void)
{
	return 0;
}

/*
 *   Yield CPU; returns TRUE if user requested an interrupt (a "control-C"
 *   type of operation to abort the entire program), FALSE to continue.
 *   Portable code should call this routine from time to time during lengthy
 *   computations that don't involve any UI operations; if practical, this
 *   routine should be invoked roughly every 10 to 100 milliseconds.
 *   
 *   The purpose of this routine is to support "cooperative multitasking"
 *   systems, such as pre-X MacOS, where it's necessary for each running
 *   program to call the operating system explicitly in order to yield the
 *   CPU from time to time to other concurrently running programs.  On
 *   cooperative multitasking systems, a program can only lose control of
 *   the CPU by making specific system calls, usually related to GUI events;
 *   a program can never lose control of the CPU asynchronously.  So, a
 *   program that performs lengthy computations without any UI interaction
 *   can cause the rest of the system to freeze up until the computations
 *   are finished; but if a compute-intensive program explicitly yields the
 *   CPU from time to time, it allows other programs to remain responsive.
 *   Yielding the CPU at least every 100 milliseconds or so will generally
 *   allow the UI to remain responsive; yielding more frequently than every
 *   10 ms or so will probably start adding noticeable overhead.
 *   
 *   On single-tasking systems (such as MS-DOS), there's only one program
 *   running at a time, so there's no need to yield the CPU; on virtually
 *   every modern system, the OS automatically schedules CPU time without
 *   the running programs having any say in the matter, so again there's no
 *   need for a program to yield the CPU.  For systems where this routine
 *   isn't needed, the system header should simply #define os_yield to
 *   something like "((void)0)" - this will allow the compiler to completely
 *   ignore calls to this routine for systems where they aren't needed.
 *   
 *   Note that this routine is NOT meant to provide scheduling hinting to
 *   modern systems with true multitasking, so a trivial implementation is
 *   fine for any modern system.  
 */
#ifndef os_yield
int os_yield(void)
{
	return 0;
}
#endif

/*
 *   Initialize the time zone.  This routine is meant to take care of any
 *   work that needs to be done prior to calling localtime() and other
 *   time-zone-dependent routines in the run-time library.  For DOS and
 *   Windows, we need to call the run-time library routine tzset() to set
 *   up the time zone from the environment; most systems shouldn't need to
 *   do anything in this routine.  
 */
#ifndef os_tzset
void os_tzset(void)
{
}
#endif

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


/* ------------------------------------------------------------------------*/
/*
 *   Translate a character from the HTML 4 Unicode character set to the
 *   current character set used for display.  Takes an HTML 4 character
 *   code and returns the appropriate local character code.
 *   
 *   The result buffer should be filled in with a null-terminated string
 *   that should be used to represent the character.  Multi-character
 *   results are possible, which may be useful for certain approximations
 *   (such as using "(c)" for the copyright symbol).
 *   
 *   Note that we only define this prototype if this symbol isn't already
 *   defined as a macro, which may be the case on some platforms.
 *   Alternatively, if the function is already defined (for example, as an
 *   inline function), the defining code can define OS_XLAT_HTML4_DEFINED,
 *   in which case we'll also omit this prototype.
 *   
 *   Important: this routine provides the *default* mapping that is used
 *   when no external character mapping file is present, and for any named
 *   entities not defined in the mapping file.  Any entities in the
 *   mapping file, if used, will override this routine.
 *   
 *   A trivial implementation of this routine (that simply returns a
 *   one-character result consisting of the original input character,
 *   truncated to eight bits if necessary) can be used if you want to
 *   require an external mapping file to be used for any game that
 *   includes HTML character entities.  The DOS version implements this
 *   routine so that games will still look reasonable when played with no
 *   mapping file present, but other systems are not required to do this.  
 */
#ifndef os_xlat_html4
# ifndef OS_XLAT_HTML4_DEFINED
void os_xlat_html4(unsigned int html4_char, char *result, size_t result_buf_len)
{
    /* Return all standard Latin-1 characters as-is */
    if (html4_char <= 128 || (html4_char >= 160 && html4_char <= 255))
        result[0] = (unsigned char)html4_char;
    else {
        switch (html4_char) {
        case 130:                                      /* single back quote */
            result[0] = '`'; break;
        case 132:                                      /* double back quote */
            result[0] = '\"'; break;
        case 153:                                             /* trade mark */
            strcpy(result, "(tm)"); return;
        case 140:                                            /* OE ligature */
        case 338:                                            /* OE ligature */
            strcpy(result, "OE"); return;
        case 339:                                            /* oe ligature */
            strcpy(result, "oe"); return;
        case 159:                                                   /* Yuml */
            result[0] = 255;
        case 376:                                        /* Y with diaresis */
            result[0] = 'Y'; break;
        case 352:                                           /* S with caron */
            result[0] = 'S'; break;
        case 353:                                           /* s with caron */
            result[0] = 's'; break;
        case 150:                                                /* en dash */
        case 8211:                                               /* en dash */
            result[0] = '-'; break;
        case 151:                                                /* em dash */
        case 8212:                                               /* em dash */
            strcpy(result, "--"); return;
        case 145:                                      /* left single quote */
        case 8216:                                     /* left single quote */
            result[0] = '`'; break;
        case 146:                                     /* right single quote */
        case 8217:                                    /* right single quote */
        case 8218:                                    /* single low-9 quote */
            result[0] = '\''; break;
        case 147:                                      /* left double quote */
        case 148:                                     /* right double quote */
        case 8220:                                     /* left double quote */
        case 8221:                                    /* right double quote */
        case 8222:                                    /* double low-9 quote */
            result[0] = '\"'; break;
        case 8224:                                                /* dagger */
        case 8225:                                         /* double dagger */
        case 8240:                                        /* per mille sign */
            result[0] = ' '; break;
        case 139:                       /* single left-pointing angle quote */
        case 8249:                      /* single left-pointing angle quote */
            result[0] = '<'; break;
        case 155:                      /* single right-pointing angle quote */
        case 8250:                     /* single right-pointing angle quote */
            result[0] = '>'; break;
        case 8482:                                           /* small tilde */
            result[0] = '~'; break;
            
        default:
            /* unmappable character - return space */
            result[0] = (unsigned char)' ';
        }
    }
    result[1] = 0;
}
# endif
#endif

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

/*
 *   Receive notification that a character mapping file has been loaded.
 *   The caller doesn't require this routine to do anything at all; this
 *   is purely for the system-dependent code's use so that it can take
 *   care of any initialization that it must do after the caller has
 *   loaded a charater mapping file.  'id' is the character set ID, and
 *   'ldesc' is the display name of the character set.  'sysinfo' is the
 *   extra system information string that is stored in the mapping file;
 *   the interpretation of this information is up to this routine.
 *   
 *   For reference, the Windows version uses the extra information as a
 *   code page identifier, and chooses its default font character set to
 *   match the code page.  On DOS, the run-time requires the player to
 *   activate an appropriate code page using a DOS command (MODE CON CP
 *   SELECT) prior to starting the run-time, so this routine doesn't do
 *   anything at all on DOS. 
 */
void os_advise_load_charmap(char *id, char *ldesc, char *sysinfo)
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
	strcpy(mapname, "asc7dflt");
}

