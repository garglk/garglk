#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/Osdos.c,v 1.3 1999/07/11 00:46:37 MJRoberts Exp $";
#endif

/*  
 *   Copyright (c) 1987, 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osdos  - Operating System dependent definitions for MS-DOS and OS/2
Function
  os_printz() - acts like fputs() to stdout
  os_print()  - prints a counted-length (not null-terminated) string
  os_waitc()  - waits for a keystroke at the keyboard
  os_flush()  - forces flush of standard output without newline
  os_getc()   - return a single character from the keyboard
  os_defext() - adds a default extension to a filename
  os_remext() - removes the extension from a filename
  os_init()   - os-dependent initialization
  os_term()   - os-dependent termination
  os_expause()- pause on exit (from ti) if this is a wacky OS (Mac, etc.)
  os_askfile()- prompt the user for a filename
  os_score()  - informs OS layer of current, maximum score (for status line)
  os_status() - sets output status:  0=normal, 1=status line, 2=room ldesc
  os_exeseek()- returns file pointer set up to read datafile from executable
  os_exfil()  - loads a resource from a file of the same name
  os_exfld()  - loads external function from an open file
  os_excall() - calls an external function
Notes
  The operating system and compiler dependent routines have been isolated
  to this module.  The appropriate pre-processor symbol for your compiler
  should be defined when you compile this module.  Currently recognized
  compilers are:

      CHIPMUNK    - Bogus Caltech Chipmunk C compiler*
      MWC         - Mark Williams C (Atari ST/TOS)
      LATTICE     - Lattice C (IBM PC/MSDOS)
      MACAZTEC    - Aztec C68k (Macintosh)
      MACLSC      - LightspeedC (Macintosh)*
      __TURBOC__  - Borland Turbo C (MSDOS)
      MICROSOFT   - Microsoft C (IBM PC/MSDOS/OS2)
      UNIX5       - Unix System 5 Portable C compiler*
      VMS         - VAX/VMS
      SUN_UNIX    - Sun version of Unix

  (Note:  those marked with an asterisk (*) are not yet fully implemented.)

  In addition, any global data items that are required for your compiler
  should be placed in this module.  Note in particular that many compilers
  set a larger-than-default stack size (which is usually required due to the
  heavy use of the stack in this program) with a global defined in this
  module.
Returns
  Varies by function.
Modified
  04/24/93 JEras         - use new os_locate() to find trcolor.dat
  07/04/92 MJRoberts     - always use BIOS video mode (usebios = 1)
  01/19/92 MJRoberts     - DOS file selector
  09/26/91 JEras         - os/2 user exit support
  09/18/91 MJRoberts     - TADS/Graphic additions
  07/19/91 MJRoberts     - deduct one from pagelength
  05/23/91 MJRoberts     - renamed os_exfrd to os_exld
  04/06/91 JEras         - os_exfrd() is for Turbo C only
  03/10/91 MJRoberts     - integrate John's qa-scripter mods
  11/23/90 JEras         - use ossgmx() to get screen size
  11/23/90 JEras         - enhance os_waitc() a little
  11/16/90 JEras         - os_init() and os_term are RUNTIME only
  10/28/90 JEras         - merge Mike's ST changes
  10/14/90 JEras         - fix shifted ldesc text after prompt bug
  10/13/90 JEras         - add USE_STDARG to avoid ref's past end of stack
  10/13/90 JEras         - fix bug in os_printf() for ldesc window
  09/14/90 SMcAdams      - make status line reverse video on mono display
  08/07/90 SMcAdams      - fix os_getc()'s use of '!'
  05/12/90 MJRoberts     - add emacs keys to history, make DOS runtime
  03/23/90 JEras         - add RUNTIME handling for DOSCOMMON and OS/2
  10/10/89 MJRoberts     - add history and editing functions
  06/25/89 MJRoberts     - add standard os_askfile, os_score, and os_status
  06/25/89 MJRoberts     - add MS-DOS ldesc display and status line
  05/30/89 MJRoberts     - add SUN_UNIX; add os_askfile
  10/26/88 MJRoberts     - add randomizer; default uses localtime()
  03/15/88 MJRoberts     - add support for Mac Aztec C, VAX/VMS
  02/02/88 A.Greenblatt  - Added support for Macintosh, Unix, Chipmunks
  01/02/88 MJRoberts     - created
*/

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include <time.h>

#ifdef TURBO
# include <sys/stat.h>
# include <dir.h>
# include <dos.h>
# define _getch getch
# define _fileno fileno
# define _fstat fstat
#endif /* TURBO */

#ifdef DJGPP
# include <signal.h>
# include <dpmi.h>
# include <fcntl.h>
# include <conio.h>
# include <sys/stat.h>
# define _getch  getch            /* use the un-underscored name for 'getch */
# define _fileno fileno                              /* ...and for 'fileno' */
# define _fstat  fstat                               /* ... and for 'fstat' */
#endif

#ifndef DJGPP
# include <share.h>
# include <dos.h>
#endif

#ifdef MICROSOFT
# ifdef T_WIN32
#  include <conio.h>
#  include <sys/stat.h>
#  include <io.h>
#  include <fcntl.h>
#  include <Windows.h>
# endif
# ifdef RUNTIME
#  include <dos.h>
# endif

/* adjust some keywords */
# if _MSC_VER >= 700
# define interrupt __interrupt
# define asm       __asm
# else
# define interrupt _interrupt
# define asm       _asm
# endif
# define getvect   _dos_getvect
# define setvect   _dos_setvect

#endif /* MICROSOFT */

#include "os.h"
#include "osgen.h"
#include "tio.h"

#ifdef __DPMI32__
# define interrupt _interrupt
#endif

#ifdef MSOS2
# define INCL_DOSMODULEMGR
# define INCL_DOSFILEMGR
# define INCL_KBD
# include <os2.h>

# ifdef RUNTIME
#  ifdef __EMX__
#   include <stdlib.h>                                           /* getcwd */
#  endif /* __EMX__ */
#  ifdef MICROSOFT
#   include <direct.h>                                           /* getcwd */
#  endif /* MICROSOFT */
#  ifdef __IBMC__
#   include <direct.h>                                           /* getcwd */
#  endif /* __IBMC__ */
# endif /* RUNTIME */

# ifdef __EMX__
#  define getch()                   _read_kbd(0, 1, 0)
# endif /* __EMX__ */
# ifdef __32BIT__
#  define DosGetProcAddr(a,b,c)     DosQueryProcAddr(a,0L,b,c)
# endif /* __32BIT__ */

#endif /* MSOS2 */

#include "tadsexit.h"

#ifndef DJGPP
#ifdef __DPMI16__
# include <windows.h>
# define SCRBASE_COLOR_PTR  ((char *)MK_FP(__SegB800, 0))
# define SCRBASE_MONO_PTR  ((char *)MK_FP(__SegB000, 0))
#else
# define SCRBASE_COLOR_PTR  ((char *)0xb8000000L)
# define SCRBASE_MONO_PTR ((char *)0xb0000000L)
#endif /* __DPMI16__ */
#endif /* !DJGPP */

/* flag: the stdin is the keyboard, so we can read from the console */
int G_stdin_is_kb = TRUE;

/* 
 *   flag: special cursor control: the UI is driving the cursor position, so
 *   don't set it according to the text buffers 
 */
static osfar_t int S_keep_cursor_pos = FALSE;

/* 
 *   Flag: use the Win95 international keyboard fix.  If this flag is set,
 *   we'll work around a set of bugs in the windows 95 keyboard driver that
 *   prevents users from entering international characters.  (We only enable
 *   the workaround when specifically asked, because the workaround itself
 *   can cause problems of its own on some systems.)  
 */
static osfar_t int S_os_win95_kb_fix = FALSE;


#ifdef USEDOSCOMMON     /* this is the DOS and OS/2 implementation */
# ifdef RUNTIME
#  if !defined(MSOS2) && !defined(DJGPP)
char *scrbase;
#  endif /* !MSOS2 && !DJGPP */


static osfar_t int oldvmode;
extern int os_f_plain;
extern void osnoui_init();
extern void osnoui_uninit();

/* initialize screen display */
void os_scrini(void)
{
    int xmax, ymax;

    /* get the BIOS parameters for the screen size */
    ossgmx(&ymax, &xmax);

    /* 
     *   set the width and height - note that ossgmx() returns the maximum
     *   column and row numbers; since these are zero-based, the width and
     *   height are (respectively) one higher than these maxima 
     */
    G_oss_screen_width = xmax + 1;
    G_oss_screen_height = ymax + 1;

    /* if we're not in 'plain' mode, initialize the display */
    if (!os_f_plain)
    {
        /* clear the screen in the default text color */
        ossclr(0, 0, ymax, xmax, text_color);
    }
}

#if defined(DJGPP) || defined(TURBO)
#define OSDOS_INIT_USEBIOS 0                    /* don't use BIOS for djgpp */
#define _stat      stat    /* use the standard Unix names for 'struct stat' */
#define _S_IFREG   S_IFREG           /* ... and for the various S_xxx flags */
#define _S_IFIFO   S_IFIFO
#endif

#ifndef OSDOS_INIT_USEBIOS
#define OSDOS_INIT_USEBIOS 1       /* unless other set, use BIOS by default */
#endif

/* special flag for using BIOS video i/o rather than screen i/o */
int usebios = OSDOS_INIT_USEBIOS;     /* set if -b flag is set in arguments */

/* flag: use stdio-based askfile rather than char-mode graphical version */
static osfar_t int use_std_askfile = 0;

int os_init(int *argc, char *argv[], const char *prompt,
            char *buf, int bufsiz)
{
    int     i;
    FILE   *fp;
    char    nbuf[128];
    int     found_arg;
    struct _stat statbuf;

    /* initialize the base non-UI package */
    osnoui_init();

    /*
     *   Scan for the -kbfix95 argument.  If we find it, set the flag to use
     *   the win95 international keyboard workaround. 
     */
    for (i = 0 ; i < *argc ; ++i)
    {
        /* check this argument */
        if (!stricmp(argv[i], "-kbfix95"))
        {
            /* set the flag to turn on the fix */
            S_os_win95_kb_fix = TRUE;

            /* 
             *   remove this argument from the option list - this is a
             *   DOS-specific option, so we don't want to pass it through to
             *   the portable code 
             */
            for ( ; i + 1 < *argc ; ++i)
                argv[i] = argv[i + 1];

            /* reduce the argument count for the removed string */
            --(*argc);

            /* no need to look at subsequent arguments */
            break;
        }
    }

#ifndef DJGPP
    scrbase = SCRBASE_COLOR_PTR;
#endif
    
    for (i = *argc - 1, found_arg = TRUE ; i != 0 && found_arg ; )
    {
        /* if this isn't a flag, we're done scanning for arguments */
        if (argv[i][0] != '-' && argv[i][0] != '/')
            break;

        /* check for one of our flags */
        switch(argv[i][1])
        {
        case 'b':
        case 'B':
            /* use BIOS for display operations */
            usebios = 1;
#if !defined(MSOS2) && !defined(DJGPP)
            scrbase = (char *)0xf0000000L;
#endif /* !MSOS2 && !DJGPP */
            break;

        case 'a':
        case 'A':
            /* use stdio version of askfile */
            use_std_askfile = TRUE;
            break;

        default:
            /* it's not one of our special added flags */
            found_arg = FALSE;
            break;
        }

        /* if this is one of our arguments, remove it */
        if (found_arg)
        {
            --i;
            --(*argc);
        }
    }

    /* initialize the console */
    oss_con_init();

    /* get the status on the standard output handle */
    if (!_fstat(_fileno(stdout), &statbuf))
    {
        /* 
         *   if the standard output has been redirected to a file, use plain
         *   mode, and turn off MORE mode at the console formatter level 
         */
        if ((statbuf.st_mode & _S_IFREG) != 0)
        {
            os_plain();
            G_os_moremode = FALSE;
        }
    }

    /* do some extra console setup in plain mode */
    if (os_f_plain)
    {
        /* get status on the standard input handle */
        if (!_fstat(_fileno(stdin), &statbuf))
        {
            /* 
             *   if the standard input has been redirected to a file, turn
             *   off MORE mode at the console formatter level 
             */
            if ((statbuf.st_mode & _S_IFREG) != 0)
                G_os_moremode = FALSE;

            /* 
             *   if the standard input is a file or a pipe, note that we must
             *   read from stdin; otherwise, assume we can read directly from
             *   the console 
             */
            if ((statbuf.st_mode & (_S_IFREG | _S_IFIFO)) != 0)
                G_stdin_is_kb = FALSE;
        }
    }

    /*
     *   If we have a monochrome monitor, change the color and screen buffer
     *   parameters accordingly.
     */
    if (ossmon())
    {
#if !defined(MSOS2) && !defined(DJGPP)
        scrbase = SCRBASE_MONO_PTR;
#endif /* !MSOS2 && !DJGPP */
        sdesc_color = 0x70;
        text_color = 7;
        ldesc_color = 0x70;
        debug_color = 7;
    }

    /* locate TRCOLOR.DAT and read the color values out of it */
    if (os_locate("trcolor.dat", 11, argv[0], nbuf, sizeof(nbuf))
        && (fp = fopen(nbuf, "r")) != 0)
    {
        int         i;
        static osfar_t int *colorptr[] =
            { &text_normal_color, &text_bold_color, &sdesc_color };

        for (i = 0 ; i < sizeof(colorptr)/sizeof(colorptr[0]) ; ++i)
        {
            fgets(nbuf, sizeof(nbuf), fp);
            *colorptr[i] = atoi(nbuf);
        }
        fclose(fp);
    }
    text_color = text_normal_color;

    /* set up the screen */
    os_scrini();

    /* initialize the status line with its background color */
    status_mode = 1;
    os_printz("");
    status_mode = 0;

#  ifdef USE_SCROLLBACK
    osssbini(16*1024);      /* allocate scrollback buffer */
#  endif /* USE_SCROLLBACK */
    
    return( 0 );
}

/*
 *   Uninitialize 
 */
void os_uninit(void)
{
    /* un-initialize the base non-UI package */
    osnoui_uninit();

    /* un-initialize the console */
    oss_con_uninit();
}

void os_term(int rc)
{
#  ifdef USE_SCROLLBACK
    osssbdel();
#  endif /* USE_SCROLLBACK */

    exit(rc);
}
# endif /* RUNTIME */

#ifdef TURBO
# define os_getch getch
#endif

#ifdef MICROSOFT
# ifdef MSDOS
#  ifndef MSOS2
#   ifndef T_WIN32

/* ------------------------------------------------------------------------ */
/* 
 *   define an os_getch that can deal with ^P, ^C, unlike MSC's getch() 
 */
char os_getch(void)
{
    char retval;

    /* put the cursor at the default input position */
    if (!S_keep_cursor_pos)
        osssb_cursor_to_default_pos();

    /* call DOS directly to get a key */
    asm {
        mov   ah, 07h
        int   21h
        mov   retval, al
    }

    /* return the key */
    return retval;
}

#   endif /* T_WIN32 */
#  endif /* MSOS2 */
# endif /* MSDOS */
#endif /* MICROSOFT */


#ifndef T_WIN32
/* ------------------------------------------------------------------------ */
/*
 *   Sleep for the given interval.  We'll simply loop until the system
 *   clock indicates the desired time. 
 */
void os_sleep_ms(long delay_in_milliseconds)
{
    long done_time;

    /* do any deferred redrawing */
    osssb_redraw_if_needed();

    /* 
     *   calculate the time when we'll be done by adding the delay to the
     *   current time 
     */
    done_time = os_get_sys_clock_ms() + delay_in_milliseconds;

    /* loop until the system clock says we're done */
    while (os_get_sys_clock_ms() < done_time)
        /* do nothing but soak up CPU cycles... */;
}


/* ------------------------------------------------------------------------ */
/*
 *   For DOS applications that aren't running as Win32 console apps,
 *   implement a keyboard read timeout by looping until the timeout
 *   expires or a key becomes available.  This hogs the CPU, but DOS
 *   doesn't provide a means of waiting for input with a timeout, so we
 *   must simply poll the keyboard.
 *   
 *   The timeout is specified in milliseconds.  
 */
static int oss_getc_internal(unsigned char *ch,
                             unsigned long timeout, int use_timeout)
{
    unsigned long abs_timeout;
    char retval;
#ifdef DJGPP
    __dpmi_regs regs;
#endif

#ifdef RUNTIME
    /* if we're in plain mode, just get a key from stdin */
    if (os_f_plain)
    {
        /* reject timeout requests */
        if (use_timeout)
            return OS_GETS_EOF;

        /* make sure output is flushed */
        fflush(stdout);

        /* get a key from stdin */
        *ch = (G_stdin_is_kb ? _getch() : getchar());

        /* return a key event */
        return OS_GETS_OK;
    }
#endif

    /* do any deferred redrawing */
    osssb_redraw_if_needed();

    /* 
     *   if we're not using a timeout, just return the next character in
     *   the ordinary fashion 
     */
    if (!use_timeout)
    {
        /* get a keystroke directly from DOS */
#ifdef DJGPP
        regs.h.ah = 7;
        __dpmi_int(0x21, &regs);
        retval = regs.h.al;
#else /* DJGPP */
        asm {
            mov   ah, 07h
            int   21h
            mov   retval, al
        }
#endif /* DJGPP */

        /* 
         *   Some keyboards return code 0340 as the first byte of an
         *   extended key sequence.  We make no use of any additional
         *   information this conveys (if indeed it conveys anything; it's
         *   not clear that it really does), so change it to zero here and
         *   now. 
         */
        if ((unsigned char)retval == 0340)
            retval = 0;

        /* return it */
        *ch = retval;
        return OS_GETS_OK;
    }

    /* 
     *   calculate the absolute time of the timeout by adding the current
     *   time to the system clock 
     */
    abs_timeout = (unsigned long)os_get_sys_clock_ms() + timeout;

    /* 
     *   loop until we reach the timeout or a keystroke arrives in the
     *   keyboard buffer 
     */
    for (;;)
    {
        /* check the keyboard buffer to see if a key is ready */
#ifdef DJGPP
        regs.h.ah = 6;
        regs.h.dl = 0xff;
        __dpmi_int(0x21, &regs);
        if (regs.x.flags & 0x40)                               /* Zero flag */
            goto no_key_available;
        retval = regs.h.al;
#else /* DJGPP */
        asm {
            mov   ah, 06h
            mov   dl, 0ffh
            int   21h
            jz    no_key_available

            mov   retval, al
        }
#endif /* DJGPP */

        /* change 0340 to 0 (same as above) */
        if ((unsigned char)retval == 0340)
            retval = 0;

        /* we got a key - put it in *ch and return success */
        *ch = retval;
        return OS_GETS_OK;

    no_key_available:
        /* 
         *   no key is ready yet - if we've reached the absolute timeout
         *   time, return with timeout 
         */
        if ((unsigned long)os_get_sys_clock_ms() >= abs_timeout)
            return OS_GETS_TIMEOUT;
    }
}

#ifdef DJGPP
/*
 *   Get a character from the keyboard 
 */
char os_getch(void)
{
    unsigned char ch;

    /* get a keystroke without a timeout */
    if (oss_getc_internal(&ch, 0, FALSE) == OS_GETS_OK)
        return ch;
    else
        return EOF;
}
#endif /* DJGPP */

/*
 *   get a character from the keyboard with a timeout - uses the internal
 *   implementation to do the work 
 */
int oss_getc_timeout(unsigned char *ch, unsigned long timeout)
{
    return oss_getc_internal(ch, timeout, TRUE);
}

#endif /* T_WIN32 */


/* ------------------------------------------------------------------------ */
/*
 *   For Win32 console apps, we need to do a whole lot more work, because
 *   console app version of the Microsoft _getch() function doesn't do a
 *   number of things we need, such as return Alt+letter keys or
 *   Alt+numeric keypad entries.  So, we'll simply implement our own
 *   console reader that takes care of all of those operations.  
 */
#ifdef T_WIN32

/*
 *   Translate a virtual key code for a numeric keypad key into the
 *   corresponding digit.  Returns a value from 0 to 9 if the key is in
 *   fact a keypad key, or something outside that range if not. 
 */
static unsigned char xlat_keypad(int vkey)
{
    switch(vkey)
    {
    case VK_HOME:
        return 7;
        
    case VK_UP:
        return 8;
        
    case VK_PRIOR:
        return 9;
        
    case VK_LEFT:
        return 4;
        
    case VK_CLEAR:
        return 5;
        
    case VK_RIGHT:
        return 6;
        
    case VK_END:
        return 1;
        
    case VK_DOWN:
        return 2;
        
    case VK_NEXT:
        return 3;
        
    case VK_INSERT:
        return 0;
        
    default:
        /* it's invalid - return something outside of the 0-9 range */
        return 100;
    }
}

/*
 *   Internal oss_getc - read a character from the keyboard, optionally
 *   timing out after the given interval.
 */
static int oss_getc_internal(unsigned char *ch,
                             unsigned long timeout, int use_timeout)
{
    static osfar_t HANDLE hcon = INVALID_HANDLE_VALUE;
    DWORD oldmode;
    static osfar_t unsigned char nxt_ret = 0;
    DWORD abs_timeout;
    int status_code;
    extern int os_f_plain;

    /* if we're in plain mode, just get a key from stdin */
    if (os_f_plain)
    {
        /* reject timeout requests */
        if (use_timeout)
            return OS_GETS_EOF;

        /* make sure stdout is flushed */
        fflush(stdout);

        /* if we're at EOF on stdin, return eof indication */
        if (feof(stdin))
            return OS_GETS_EOF;

        /* get a key */
        *ch = (G_stdin_is_kb ? _getch() : getchar());

        /* return a key event */
        return OS_GETS_OK;
    }

    /* 
     *   if we have a timeout value, calculate the wall clock time when it
     *   expires 
     */
    if (use_timeout)
        abs_timeout = GetTickCount() + timeout;

    /* if we've buffered up a character, return it */
    if (nxt_ret != 0)
    {
        *ch = nxt_ret;
        nxt_ret = 0;
        return OS_GETS_OK;
    }

    /* open the console if we haven't already done so */
    if (hcon == INVALID_HANDLE_VALUE)
    {
        hcon = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                          OPEN_EXISTING, 0, 0);
        if (hcon == INVALID_HANDLE_VALUE)
        {
            *ch = _getch();
            return OS_GETS_OK;
        }
    }

    /* save the original console mode for restoration before we return */
    GetConsoleMode(hcon, &oldmode);

    /* set our console mode */
    SetConsoleMode(hcon, ENABLE_WINDOW_INPUT);

    /* read characters until we get something we like */
    for (;;)
    {
        INPUT_RECORD inrec;
        DWORD reccnt;
        KEY_EVENT_RECORD *keyevt;
        int alt, ctl, shift, extended;

        /* 
         *   if we have a timeout value, wait for something to become
         *   available 
         */
        if (use_timeout)
        {
            DWORD cur_time;
            DWORD interval;
            
            /* check to see if we've already reached the timeout */
            cur_time = GetTickCount();
            if (cur_time >= abs_timeout)
            {
                /* we've reached the timeout */
                status_code = OS_GETS_TIMEOUT;
                goto done;
            }

            /* 
             *   wait for input to become available, limiting our wait to
             *   the interval remaining before the absolute timeout
             */
            interval = (DWORD)(abs_timeout - cur_time);
            if (WaitForSingleObject(hcon, interval) == WAIT_TIMEOUT)
            {
                /* the timeout expired - return timeout */
                status_code = OS_GETS_TIMEOUT;
                goto done;
            }
        }

        /*
         *   If we're using the win95 international keyboard fix, check the
         *   next event to see if we should read it using ReadConsole or
         *   ReadConsoleInput.  Each call has weird limitations, so we must
         *   check the type of event we have to determine which set of
         *   limitations we can live with for the event.  Note that this is
         *   only a problem in win95; in 98 and later versions, as well as
         *   NT, ReadConsoleInput works properly in all cases, so we can use
         *   it as the single keyboard input reader.
         *   
         *   The problem we're trying to solve is that ReadConsoleInput on
         *   windows 95 does not properly handle keyboard layouts on
         *   localized systems.  There are two facets to the problem.
         *   First, ReadConsoleInput prevents the special key events that
         *   switch keyboard layouts from reaching the keyboard layout
         *   manager; this makes it impossible for the user to select
         *   different keyboard layouts, which is necessary for using
         *   certain character sets.  Second, ReadConsoleInput does not
         *   properly translate the keystrokes through the keyboard layout,
         *   so even when the proper layout is selected, the wrong character
         *   codes are returned.
         *   
         *   ReadConsole on win95 does not suffer from these problems; it
         *   seems to be layered above the keyboard layout manager, so the
         *   layout manager has a chance to intercept keystrokes for layout
         *   change detection and for character translation.  Unfortunately,
         *   ReadConsole doesn't provide any way to read extended keys such
         *   as arrows and function keys.
         *   
         *   Note that this fix might be problematic on some systems.
         *   Microsoft support specifically warns in a tech note on the msdn
         *   site that calls to ReadConsole cannot be mixed with calls to
         *   ReadConsoleInput on a single console handle; the usual
         *   "unpredictable results" are said to apply.  However, in
         *   practice, it seems that this isn't really that big a problem
         *   for most users, and other applications have reportedly been
         *   using this same mixed-API fix for some time.  So, we'll code
         *   this fix, but we'll use it only when the user explicitly
         *   enables it; this seems to be the safest approach, in that it
         *   ensures that users who don't already have keyboard problems
         *   won't start having them (since they'll never enable the fix),
         *   and those that do might have a way to work around them.  
         */
        if (S_os_win95_kb_fix)
        {
            int evt_ch;
            
            /* 
             *   wait for an event to become available (since Peek doesn't
             *   wait by itself) 
             */
            WaitForSingleObject(hcon, INFINITE);

            /* look at (but do not yet actually read) the next event */
            PeekConsoleInput(hcon, &inrec, 1, &reccnt);

            /* presume this will be a key event (no harm done if not) */
            keyevt = &inrec.Event.KeyEvent;

            /* get the ALT-key state */
            alt = ((keyevt->dwControlKeyState
                    & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) != 0);

            /* get the character from the event, if applicable */
            evt_ch = keyevt->uChar.AsciiChar;

            /* 
             *   If we have a keyboard event, and the event contains a
             *   regular printable character, use ReadConsole() to read the
             *   event.  If the ALT key is down, consider it a regular
             *   character if the AsciiChar is outside of the [A-Za-z] range
             *   - if it's in the regular alphabetic range, treat it as an
             *   ALT+letter command sequence rather than as a localized
             *   character entry.  
             */
            if (inrec.EventType == KEY_EVENT
                && keyevt->bKeyDown
                && evt_ch != '\0'
                && (!alt || (alt && evt_ch > 127)))
            {
                /* 
                 *   it's a regular character - use ReadConsole to read the
                 *   keystroke, since it performs the proper translations 
                 */
                ReadConsole(hcon, ch, 1, &reccnt, 0);

                /* return this character */
                status_code = OS_GETS_OK;
                goto done;
            }
        }
        
        /* read a console event */
        if (!ReadConsoleInput(hcon, &inrec, 1, &reccnt))
        {
            *ch = _getch();
            status_code = OS_GETS_OK;
            goto done;
        }

        /* see what we have */
        switch(inrec.EventType)
        {
#ifdef RUNTIME
        case WINDOW_BUFFER_SIZE_EVENT:
            /* notify the ossdos32 layer of the change */
            oss_win_resize_event();
            break;
#endif
            
        case KEY_EVENT:
            /* note that we have a key event */
            status_code = OS_GETS_OK;

            /* get the key event */
            keyevt = &inrec.Event.KeyEvent;

            /* ignore key-up events */
            if (!keyevt->bKeyDown)
                continue;

            /* if it's a shift key of some kind, ignore it */
            switch(keyevt->wVirtualKeyCode)
            {
            case VK_SHIFT:
            case VK_CONTROL:
            case VK_MENU:
            case VK_PAUSE:
            case VK_CAPITAL:
            case VK_NUMLOCK:
            case VK_SCROLL:
                continue;
            }

            /* get the control key state */
            alt = ((keyevt->dwControlKeyState
                    & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) != 0);
            ctl = ((keyevt->dwControlKeyState
                    & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) != 0);
            shift = ((keyevt->dwControlKeyState & SHIFT_PRESSED) != 0);
            extended = ((keyevt->dwControlKeyState & ENHANCED_KEY) != 0);

            /* if it's an ascii character, return it directly */
            if (keyevt->uChar.AsciiChar != 0)
            {
                unsigned char evtch = keyevt->uChar.AsciiChar;

                /* if the ALT key is down, translate it */
                if (alt)
                {
                    /* check to see if it's alphabetic */
                    if (evtch >= 'a' && evtch <= 'z')
                        evtch -= 'a' - 'A';
                    if (evtch >= 'A' && evtch <= 'Z')
                    {
                        static osfar_t char alt_alpha[] =
                        {
                            30, 48, 46, 32, 18, 33, 34, 35, 23, 36,
                            37, 38, 50, 49, 24, 25, 16, 19, 31, 20,
                            22, 47, 17, 45, 21, 44
                        };

                        /* 
                         *   translate it to the traditional BIOS ALT-key
                         *   code and return it as a two-character
                         *   sequence 
                         */
                        nxt_ret = alt_alpha[evtch - 'A'];
                        *ch = 0;
                        status_code = OS_GETS_OK;
                        goto done;
                    }

                    /* ignore other ALT key combinations */
                    break;
                }
                
                /* it's a normal ascii character - return it as-is */
                *ch = evtch;
                status_code = OS_GETS_OK;
                goto done;
            }

            /*
             *   If the ALT key is down, and we're typing on the numeric
             *   keypad, gather up keys for a keypad sequence.  We can
             *   tell that we're typing on the keypad by checking to see
             *   if we're hitting Home, Up, PgUp, Left, Right, End, Down,
             *   PgDn, Ins, or Clear, and we're not typing an extended
             *   key. 
             */
            if (alt && !ctl && !shift && !extended)
            {
                /* make sure we're typing a keypad key */
                *ch = xlat_keypad(keyevt->wVirtualKeyCode);
                if (*ch < 10)
                {
                    unsigned ignore_vkey;
                    int ansi_key_seq;

                    /* 
                     *   If the first key is a '0', it indicates an ANSI code
                     *   sequence rather than an OEM code sequence, in which
                     *   case we need to translate the keystroke to the OEM
                     *   set we use internally.  For now, simply note it;
                     *   we'll handle the translation later.
                     *   
                     *   This leading '0' business is a Windows convention -
                     *   it's not something we invented.  Windows has the
                     *   weird problem that console windows and regular
                     *   Windows apps tend to use different character sets,
                     *   so they came up with this leading '0' as a way to
                     *   let the user indicate they want to enter an "ANSI"
                     *   character (the name for the character set that
                     *   Windows apps use, even though it's not actually
                     *   ANSI) rather than the default, which is "OEM" (the
                     *   set that DOS boxes use).  
                     */
                    ansi_key_seq = (*ch == 0);

                    /*
                     *   If we're using the ReadConsole keyboard fix for
                     *   win95, simply read the event using ReadConsole.  
                     */
                    if (S_os_win95_kb_fix)
                    {
                        /* get the event */
                        ReadConsole(hcon, ch, 1, &reccnt, 0);

                        /* return it */
                        status_code = OS_GETS_OK;
                        goto done;
                    }

                    /* 
                     *   ignore repeats of this key until we see a key-up
                     *   for it 
                     */
                    ignore_vkey = keyevt->wVirtualKeyCode;

                    /* keep going until we stop hitting keypad keys */
                    for (;;)
                    {
                        unsigned char nxt;
                        
                        /* peek at the next event */
                        WaitForSingleObject(hcon, INFINITE);
                        if (!PeekConsoleInput(hcon, &inrec, 1, &reccnt)
                            || reccnt == 0)
                            break;

                        /* if it's not a keyboard event, we're done */
                        if (inrec.EventType != KEY_EVENT)
                            break;

                        /* if it's a key-up for the ALT key, we're done */
                        if (!keyevt->bKeyDown
                            && keyevt->wVirtualKeyCode == VK_MENU)
                        {
                            /* done reading the key */
                            break;
                        }

                        /* 
                         *   if it's a key-up for the 'ignore' key, we can
                         *   stop ignoring this key again 
                         */
                        if (!keyevt->bKeyDown
                            && keyevt->wVirtualKeyCode == ignore_vkey)
                            ignore_vkey = 0;

                        /* if it's another key up, ignore it */
                        if (!keyevt->bKeyDown)
                            goto skip_event;

                        /* get the control key state */
                        alt = ((keyevt->dwControlKeyState
                                & (RIGHT_ALT_PRESSED
                                   | LEFT_ALT_PRESSED)) != 0);
                        ctl = ((keyevt->dwControlKeyState
                                & (RIGHT_CTRL_PRESSED
                                   | LEFT_CTRL_PRESSED)) != 0);
                        shift = ((keyevt->dwControlKeyState
                                  & SHIFT_PRESSED) != 0);
                        extended = ((keyevt->dwControlKeyState
                                     & ENHANCED_KEY) != 0);

                        /* if the shift keys have changed, we're done */
                        if (!alt || ctl || shift || extended)
                            break;

                        /* 
                         *   if it's a key down for the same key as last
                         *   time, and we haven't gotten a key-up for this
                         *   key yet, it's an auto-repeat - ignore it 
                         */
                        if (keyevt->wVirtualKeyCode == ignore_vkey)
                            goto skip_event;

                        /* ignore auto-repeats of this same key */
                        ignore_vkey = keyevt->wVirtualKeyCode;

                        /* if it's not a keypad key, we're done */
                        nxt = xlat_keypad(keyevt->wVirtualKeyCode);
                        if (nxt > 9)
                            break;

                        /* add this into the accumulator */
                        *ch *= 10;
                        *ch += nxt;

                    skip_event:
                        /* read and discard this event */
                        ReadConsoleInput(hcon, &inrec, 1, &reccnt);
                    }

                    /* 
                     *   if the character was specified as an ANSI character,
                     *   translate it to the OEM character set 
                     */
                    if (ansi_key_seq)
                    {
                        char buf[2];

                        /* set up a null-terminated string for translation */
                        buf[0] = *ch;
                        buf[1] = '\0';

                        /* translate it */
                        CharToOem(buf, buf);

                        /* 
                         *   if we didn't get a character back, then the ANSI
                         *   character must have no representation in the OEM
                         *   character set; simply ignore the character 
                         */
                        if (buf[0] == '\0')
                            break;

                        /* use the OEM representation */
                        *ch = buf[0];
                    }

                    /* return the key code */
                    status_code = OS_GETS_OK;
                    goto done;
                }
            }

            /* 
             *   if none of the shift keys are down, return the key as an
             *   extended two-byte key code 
             */
            if (!alt && !shift && !ctl)
            {
                /* 
                 *   Return it as a two-character code; the virtual scan
                 *   key corresponds to the traditional BIOS extended key
                 *   codes, so use the virtual scan code as the second
                 *   byte of the two-byte sequence 
                 */
                *ch = 0;
                nxt_ret = (unsigned char)keyevt->wVirtualScanCode;
                status_code = OS_GETS_OK;
                goto done;
            }

            /* check for shift key combinations */
            if (shift && !ctl && !alt)
            {
                /* presume we'll return a two-byte sequence */
                *ch = 0;

                /* see what we have */
                switch(keyevt->wVirtualKeyCode)
                {
                case VK_F1:
                case VK_F2:
                case VK_F3:
                case VK_F4:
                case VK_F5:
                case VK_F6:
                case VK_F7:
                case VK_F8:
                case VK_F9:
                case VK_F10:
                case VK_F11:
                case VK_F12:
                    nxt_ret = 84 + keyevt->wVirtualKeyCode - VK_F1;
                    goto done;

                default:
                    break;
                }
            }

            /* check for control key combinations */
            if (ctl && !alt && !shift)
            {
                /* presume we'll return a two-byte sequence */
                *ch = 0;

                /* see what we have */
                switch(keyevt->wVirtualKeyCode)
                {
                case VK_PRIOR:
                    nxt_ret = 132;
                    goto done;
                    
                case VK_NEXT:
                    nxt_ret = 118;
                    goto done;
                    
                case VK_LEFT:
                    nxt_ret = 115;
                    goto done;
                    
                case VK_RIGHT:
                    nxt_ret = 116;
                    goto done;
                    
                case VK_HOME:
                    nxt_ret = 119;
                    goto done;
                    
                case VK_END:
                    nxt_ret = 0165;
                    goto done;

                default:
                    break;
                }
            }
            
            break;

        default:
            /* ignore other event types */
            break;
        }
    }

done:
    /* restore the original console mode */
    SetConsoleMode(hcon, oldmode);

    /* return the result */
    return status_code;
}

/*
 *   Get a character from the keyboard 
 */
char os_getch(void)
{
    unsigned char ch;

    /* get a keystroke without a timeout */
    if (oss_getc_internal(&ch, 0, FALSE) == OS_GETS_OK)
        return ch;
    else
        return EOF;
}

/*
 *   Get a character from the keyboard, timing out after the given
 *   interval 
 */
int oss_getc_timeout(unsigned char *ch, unsigned long timeout)
{
    /* get a keystroke using the given timeout */
    return oss_getc_internal(ch, timeout, TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Sleep until the specified time.  This is especially easy for Win32
 *   console apps, since we can just call the system API to do exactly
 *   this.  
 */
void os_sleep_ms(long delay_in_milliseconds)
{
    /* do any deferred redrawing */
    osssb_redraw_if_needed();

    /* put the cursor at the default input position while pausing */
    if (!S_keep_cursor_pos)
        osssb_cursor_to_default_pos();

    /* sleep for the desired delay */
    Sleep(delay_in_milliseconds);
}


#endif /* T_WIN32 */

/* ------------------------------------------------------------------------ */
#ifndef USE_DOOR
# ifndef WINDOWS
void os_waitc(void)
{
    int c;
    
    /* read a character */
    c = os_getch();

    /* read the second byte if it's a two-byte sequence */
    if ((c == 0 || (unsigned char)c == 0340))
        os_getch();
}
# endif /* !WINDOWS */
#endif /* !USE_DOOR */

/* ------------------------------------------------------------------------ */

#ifndef USE_DOOR
# ifndef WINDOWS
#  ifndef DJGPP
int os_kbhit(void)
{
#   ifdef MSOS2
    KBDKEYINFO  ki;

    return( KbdPeek(&ki, (HKBD)0) == 0 && (ki.fbStatus & 0x40) );
#   else /* !MSOS2 */
    int ret = 0;
    
#ifndef __DPMI32__
#ifndef T_WIN32
    asm {
        push bp
        push di
        push si
        push ds
        push es

        mov ah, 1
        int 16h
        jz  no_key_ready
    }
    ret = 1;
no_key_ready:
    asm {
        pop es
        pop ds
        pop si
        pop di
        pop bp
    }
#endif /* T_WIN32 */
#endif /* __DPMI32__ */
    return(ret);
#   endif /* MSOS2 */
}
#  endif /* !DJGPP */

/* ------------------------------------------------------------------------ */
/*
 *   Read a character from the keyboard 
 */

#ifdef RUNTIME
/* ALT+letter function keys for PC, in A to Z order */
char osfar_t altkeys[] =
{
    30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50,
    49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44
};

/* 
 *   Flag: the next keystroke to be read from the keyboard is the second
 *   half of a two-character command key sequence.  This is used by the
 *   various high-level keyboard readers to remember that an extended
 *   command sequence is being read, hence the second keystroke must be
 *   interpreted as a command key.
 *   
 *   If this value is zero, it means that no command is pending.  If this
 *   value is non-zero, it contains the command code that should be
 *   returned on the next call to os_getc() or other keyboard readers.  
 */
static osfar_t int oss_command_pending;

/*
 *   Interpret the second character of a two-character keystroke into a
 *   command code, and store the command in oss_command_pending.
 *   os_getc() and other similar routines use this to figure out the
 *   portable meaning of the second character of an extended keystroke
 *   sequence.
 *   
 *   This returns one of the following values:
 *   
 *   OSS_ICK_NORMAL - it's a normal key; the caller should simply return
 *   this key to its caller.
 *   
 *   OSS_ICK_CMD - it's a single-character BIOS keystroke, but a
 *   two-character portable command code.  We will store the command code
 *   for later retrieval; the caller should simply return 0 to its caller.
 *   
 *   OSS_ICK_NEED_2ND - another keyboard character is needed to complete
 *   the keystroke.  This is returned when the first key is a BIOS-level
 *   two-character keystroke header.  The caller should call
 *   oss_interpret_2nd_key() with the next BIOS-level character and return
 *   0 to its caller to indicate that a command is in progress.
 *   
 *   OSS_ICK_BAD - invalid command sequence; the command has no portable
 *   interpretation, so the entire sequence should simply be ignored.  The
 *   caller should suppress the keystroke and get another key from the
 *   keyboard.  
 */
#define OSS_ICK_NORMAL     0
#define OSS_ICK_CMD        1
#define OSS_ICK_NEED_2ND   2
#define OSS_ICK_BAD        3
static int oss_interpret_command_key(int c, int translated)
{
    /* presume we won't find a command value */
    oss_command_pending = 0;

    /* check for an extended keycode */
    if (c == 0)
    {
        /* extended BIOS keystroke - we need a second keystroke */
        return OSS_ICK_NEED_2ND;
    }
    else if (c >= 32 && c <= 255)
    {
        /* it's a normal keystroke */
        return OSS_ICK_NORMAL;
    }
    else if (translated)
    {
        /* check to see if it's a special single-character keystroke */
        switch(c)
        {
        case 1:                                                       /* ^A */
            oss_command_pending = CMD_HOME;
            break;
        case 2:                                                       /* ^B */
            oss_command_pending = CMD_LEFT;
            break;
        case 4:                                                       /* ^D */
            oss_command_pending = CMD_DEL;
            break;
        case 5:                                                       /* ^E */
            oss_command_pending = CMD_END;
            break;
        case 6:                                                       /* ^F */
            oss_command_pending = CMD_RIGHT;
            break;
        case '\t':
            oss_command_pending = CMD_TAB;
            break;
        case 11:                                                      /* ^K */
            oss_command_pending = CMD_DEOL;
            break;
        case 14:                                                      /* ^N */
            oss_command_pending = CMD_DOWN;
            break;
        case 16:                                                      /* ^P */
            oss_command_pending = CMD_UP;
            break;
        case 21:                                                      /* ^U */
        case 27:                                                  /* Escape */
            oss_command_pending = CMD_KILL;
            break;

        case 13:
        case 8:
            /* normal keystrokes */
            return OSS_ICK_NORMAL;
        }

        /* 
         *   if we selected a command, indicate that this is a command
         *   key; otherwise, it's invalid 
         */
        if (oss_command_pending != 0)
            return OSS_ICK_CMD;
        else
            return OSS_ICK_BAD;
    }
    else
    {
        /* 
         *   we want a raw keystroke, so return a control/escape key
         *   as-is, without command mapping 
         */
        return OSS_ICK_NORMAL;
    }
}

/*
 *   Interpret the second key of a two-character keystroke.  When
 *   oss_interpret_command_key() returns OSS_ICK_NEED_2ND, the caller
 *   should get another key from the low-level keyboard handler and call
 *   this routine with the second keystroke.  We'll return:
 *   
 *   OSS_ICK_CMD - we've stored a command.  The caller should return 0 to
 *   indicate to its caller that a two-character command sequence is in
 *   progress.  We will store the command code in oss_command_pending for
 *   later retrieval.
 *   
 *   OSS_ICK_BAD - invalid command.  The caller should ignore the key and
 *   get another.  
 */
static int oss_interpret_2nd_key(int c, int translated)
{
    int   i;
    char *p;

    /* check for alt keys */
    for (i = 0, p = altkeys ; i < 26 ; ++p, ++i)
    {
        if (c == *p)
        {
            /* set the command */
            oss_command_pending = CMD_ALT + i;

            /* indicate that we found a command */
            return OSS_ICK_CMD;
        }
    }

    /* check the extended keycode */
    switch(c)
    {
    case 0110:                                                  /* Up Arrow */
        oss_command_pending = CMD_UP;
        break;
    case 0120:                                                /* Down Arrow */
        oss_command_pending = CMD_DOWN;
        break;
    case 0115:                                               /* Right Arrow */
        oss_command_pending = CMD_RIGHT;
        break;
    case 0113:                                                /* Left Arrow */
        oss_command_pending = CMD_LEFT;
        break;
    case 0117:                                                       /* End */
        oss_command_pending = CMD_END;
        break;
    case 0107:                                                      /* Home */
        oss_command_pending = CMD_HOME;
        break;
    case 0165:                                                  /* Ctrl-End */
        oss_command_pending = CMD_DEOL;
        break;
    case 0123:                                                       /* Del */
        oss_command_pending = CMD_DEL;
        break;
    case 073:                                                         /* F1 */
        /* 
         *   if they want a translated keystroke, return the SCROLL
         *   command; otherwise, just return the plain F1 key 
         */
        if (translated)
            oss_command_pending = CMD_SCR;
        else
            oss_command_pending = CMD_F1;
        break;
    case 0111:                                                      /* PgUp */
        oss_command_pending = CMD_PGUP;
        break;
    case 0121:                                                      /* PgDn */
        oss_command_pending = CMD_PGDN;
        break;
    case 0122:
        oss_command_pending = CMD_INS;                            /* Insert */
        break;
    case 132:                                               /* control-PgUp */
        oss_command_pending = CMD_TOP;
        break;
    case 118:                                               /* control-PgDn */
        oss_command_pending = CMD_BOT;
        break;
    case 119:                                               /* control home */
        oss_command_pending = CMD_CHOME;
        break;
    case 115:                                         /* control left arrow */
        oss_command_pending = CMD_WORD_LEFT;
        break;
    case 116:                                        /* control right arrow */
        oss_command_pending = CMD_WORD_RIGHT;
        break;

    case 60: oss_command_pending = CMD_F2; break;
    case 61: oss_command_pending = CMD_F3; break;
    case 62: oss_command_pending = CMD_F4; break;
    case 63: oss_command_pending = CMD_F5; break;
    case 64: oss_command_pending = CMD_F6; break;
    case 65: oss_command_pending = CMD_F7; break;
    case 66: oss_command_pending = CMD_F8; break;
    case 67: oss_command_pending = CMD_F9; break;
    case 68: oss_command_pending = CMD_F10; break;

    case 85: oss_command_pending = CMD_SF2; break;            /* shifted F2 */

    default:                                   /* Unrecognized function key */
        oss_command_pending = 0;
        break;
    }

    /* 
     *   if we set a command code, indicate to the caller that we found a
     *   command; otherwise, it's not a valid keystroke 
     */
    if (oss_command_pending != 0)
        return OSS_ICK_CMD;
    else
        return OSS_ICK_BAD;
}

/*
 *   Return the pending command, clearing the pending command flag in the
 *   process.  os_getc and similar keyboard readers should check
 *   oss_command_pending, and if that value is non-zero simply return the
 *   result of calling this function. 
 */
int oss_get_pending_command()
{
    int ret;

    /* remember the value to return */
    ret = oss_command_pending;

    /* clear the pending command, now that we've read it */
    oss_command_pending = 0;

    /* return the command */
    return ret;
}

/*
 *   Get a key from the keyboard.  If translated is true, we'll return the
 *   high-level CMD_xxx code for the key if it has both a translated and
 *   raw equivalent; otherwise, we'll return the raw CMD_xxx code for the
 *   key.  
 */
static int os_getc_internal(unsigned char *ch,
                            unsigned long timeout, int use_timeout,
                            int translated)
{
    int status;

    /* do any pending redraw */
    osssb_redraw_if_needed();

    /* put the cursor at the default input position */
    if (!S_keep_cursor_pos)
        osssb_cursor_to_default_pos();

    /* if we have a command stored up from last time, read it now */
    if (oss_command_pending != 0)
    {
        *ch = oss_get_pending_command();
        return OS_GETS_OK;
    }

    /* keep going until we get something good */
    for (;;)
    {
        /* get a key */
        status = oss_getc_internal(ch, timeout, use_timeout);

        /* check the status */
        switch(status)
        {
        case OS_GETS_OK:
            /* got a key - process it normally */
            break;

        default:
            /* anything else is an error - return it */
            return status;
        }
        
        /* check to see if it's a command sequence */
        switch(oss_interpret_command_key(*ch, translated))
        {
        case OSS_ICK_BAD:
            /* invalid key - ignore it and get another */
            break;
            
        case OSS_ICK_NORMAL:
            /* it's a normal key - just return it */
            return OS_GETS_OK;

        case OSS_ICK_CMD:
            /* 
             *   it's a single-key command - return zero to indicate to
             *   the caller that they must ask for a second key 
             */
            *ch = 0;
            return OS_GETS_OK;

        case OSS_ICK_NEED_2ND:
            /* 
             *   We need a second character code.  Since the second
             *   character is really part of the same keystroke event,
             *   there should be no delay in getting it, so don't bother
             *   using a timeout.  
             */
            oss_getc_internal(ch, 0, FALSE);
            switch(oss_interpret_2nd_key(*ch, translated))
            {
            case OSS_ICK_BAD:
                /* invalid command - ignore it and get another keystroke */
                break;
                
            case OSS_ICK_CMD:
                /* 
                 *   it's a command sequence - return zero to indicate to
                 *   the caller that they must ask for a second key 
                 */
                *ch = 0;
                return OS_GETS_OK;
            }
        }
    }
}

/*
 *   Get a keystroke with a time limit 
 */
int os_getc_timeout(unsigned char *ch, unsigned long timeout)
{
    /* call the internal routine, using the timeout */
    return os_getc_internal(ch, timeout, TRUE, TRUE);
}

/*
 *   Get a keystroke from the keyboard 
 */
int os_getc(void)
{
    unsigned char ch;

    /* call the internal routine, with no timeout */
    os_getc_internal(&ch, 0, FALSE, TRUE);

    /* return the character we read */
    return ch;
}

/*
 *   Get a raw keystroke from the keyboard
 */
int os_getc_raw(void)
{
    unsigned char ch;

    /* call the internal routine, with no timeout */
    os_getc_internal(&ch, 0, FALSE, FALSE);

    /* return the character we read */
    return ch;
}

/*
 *   Get an event.  The only types of events we provide currently are KEY
 *   and TIMEOUT events.  
 */
int os_get_event(unsigned long timeout_in_milliseconds, int use_timeout,
                 os_event_info_t *info)
{
    int kbstat;
    unsigned char ch;
    
    /* get a key, using the timeout if specified */
    kbstat = os_getc_internal(&ch, timeout_in_milliseconds, use_timeout,
                              FALSE);

    /* if that timed out, return a timeout event */
    if (kbstat == OS_GETS_TIMEOUT)
        return OS_EVT_TIMEOUT;

    /* if we got a key, return it */
    if (kbstat == OS_GETS_OK)
    {
        /* store the first character */
        info->key[0] = (int)ch;

        /* 
         *   if there's an extended character code coming, get it - note
         *   that we don't need to bother with a timeout this time around,
         *   since the second half of the key code is not a separate event
         *   and is thus not subject to timing out 
         */
        if (ch == 0)
            info->key[1] = os_getc();
        
        /* return the keyboard event */
        return OS_EVT_KEY;
    }

    /* failure - return an error indication */
    return OS_EVT_EOF;
}


#else /* RUNTIME */

/* 
 *   initialization
 */
int os_init(int *argc, char **argv, const char *prompt, char *buf, int bufsiz)
{
    return 0;
}

/*
 *   uninitialization
 */
void os_uninit()
{
}

/*
 *   Get a key from the keyboard - simple non-RUNTIME version 
 */
int os_getc(void)
{
    int c;

    /* keep going until we get a valid key */
    for (;;)
    {
        /* get a key */
        c = os_getch();

        /* if it's valid, return it */
        if ((c & 0xff) != 0)
            return c;
    }
}

/*
 *   Get a raw (untranslated) key from the keyboard - simple non-RUNTIME
 *   version 
 */
int os_getc_raw(void)
{
    /* os_getc() doesn't perform any translation, so just call it */
    return os_getc();
}

#endif /* RUNTIME */

# endif /* !WINDOWS */
#endif /* !USE_DOOR */

extern unsigned long ossgetdisks(void);
# ifdef RUNTIME
#  if (!defined(WINDOWS) && !defined(_Windows)) || defined(__DPMI16__)
#   if defined(TURBO) || defined(DJGPP)
/*
 *   implementations of library functions for directory operations -
 *   these may be Turbo-specific, so they're ifdef'd TURBO 
 */

/* maximum length of a fully-qualified filename */
#   define OSS_MAXPATH MAXPATH

/* get current working directory into buffer */
#   ifdef TURBO
#    define osspwd(buf, buflen) ((void)getcwd(buf, buflen))
#   endif

/* change directory */
#   define osscd(path) ((void)chdir(path))

/* set disk:  0 = A:, 1 = B:, 2 = C:, etc */
#   define osssetdisk(drivenum) ((void)setdisk(drivenum))

/* get disk:  0 = A:, 1 = B:, 2 = C:, etc */
#   define ossgetdisk() getdisk()

/* size of a file entry, in bytes */
#   define OSSFNAMELEN 14

#   ifdef DJGPP
/*
 *   get current working directory into buffer replace all '/' with '\\' 
 */
static void osspwd(char *buf, int buflen)
{
    char *p;

    getcwd(buf, buflen);
    for (p = buf; *p; p++)
        if (*p == '/') *p = '\\';
}
#   endif /* DJGPP */
#  endif /* TURBO || DJGPP */

#  ifdef MSOS2
/*
 *   implementations of library functions for directory operations -
 *   these are OS/2-specific, so they're ifdef'd MSOS2
 */

/* maximum length of a fully-qualified filename */
#   define OSS_MAXPATH 128

/* get current working directory into buffer */
#   define osspwd(buf, buflen) ((void)getcwd(buf, buflen))
/* context structure used for file searches */
#ifdef __32BIT__
# ifndef __IBMC__
#  define DosSelectDisk(a)      DosSetDefaultDisk(a)
#  define DosQCurDisk(a,b)      DosQueryCurrentDisk(a,b)
# endif /* !__IBMC__ */
typedef ULONG           drive_t;
#else /* !__32BIT__ */
# define DosFindFirst(a,b,c,d,e,f)  DosFindFirst(a,b,c,d,e,f,0L)
typedef USHORT          drive_t;
#endif /* __32BIT__ */

/* change directory */
#   define osscd(path) ((void)chdir(path))

/* set disk:  0 = A:, 1 = B:, 2 = C:, etc */
static void osssetdisk(int drv)
{
    DosSelectDisk((drive_t)(drv+1));
}

/* get disk:  0 = A:, 1 = B:, 2 = C:, etc */
static int ossgetdisk(void)
{
    drive_t         drv;
    ULONG           map;                                          /* dummy */

    return( DosQCurDisk(&drv, &map) ? -1 : (int)(drv-1) );
}

/* size of a file entry, in bytes */
#   define OSSFNAMELEN 14

#  else /* MSOS2 */
#   ifdef MICROSOFT

#    ifdef T_WIN32

#include <io.h>
#include <direct.h>

#define OSS_MAXPATH _MAX_PATH
#define OSSFNAMELEN _MAX_FNAME
#define osspwd(buf, buflen) ((void)_getcwd(buf, buflen))

#define ossgetdisk() (_getdrive() - 1)
#define osssetdisk(disk) _chdrive((disk) + 1)
#define osscd(path) _chdir(path)

#    else /* T_WIN32 */

/* definitions for Microsoft C on DOS */
#   define OSS_MAXPATH 128

/* get current working directory into buffer */
#   define osspwd(buf, buflen) ((void)getcwd(buf, buflen))

/* change directory */
#   define osscd(path) ((void)chdir(path))

static int ossgetdisk(void)
{
    unsigned int drive;
    _dos_getdrive(&drive);
    return(drive + 1);
}

static void osssetdisk(int drivenum)
{
    unsigned int maxdrives;
    _dos_setdrive(drivenum + 1, &maxdrives);
}

/* size of a file entry, in bytes */
#  define OSSFNAMELEN 14

#    endif /* T_WIN32 */
#   endif /* MICROSOFT */
#  endif /* MSOS2 */

/* advance by one file in a file list */
static char *fwd_file(char *cur)
{
    return(cur + OSSFNAMELEN);
}

/* back up by one file in a null-terminated list */
static char *backup_file(char *cur, char *first)
{
    if (cur == first)
        return cur;
    else
        return cur - OSSFNAMELEN;
}

/* filename comparison routine, for directory sorter */
static int fnamecmp(const void *a0, const void *b0)
{
    char *a = (char *)a0;
    char *b = (char *)b0;
    int atyp, btyp;
    char c;

    /* 
     *   Get the type of the first name.  If it ends with a colon, it's a
     *   drive letter, so it's of type 0; if it ends with a backslash, it's
     *   a subdirectory, so it's of type 1; otherwise, it's an ordinary
     *   filename, of type 2. 
     */
    c = a[strlen(a) - 1];
    atyp = (c == ':' ? 0 : c == '\\' ? 1 : 2);

    /* get hte type of the second name */
    c = b[strlen(b) - 1];
    btyp = (c == ':' ? 0 : c == '\\' ? 1 : 2);

    /*
     *   Broadly, we sort disk names first, then directory names, then
     *   filenames.  So, if we have different types of objects, simply sort
     *   by group.  
     */
    if (atyp != btyp)
        return atyp - btyp;

    /* 
     *   if they're directories, put the special "." and ".." entries first,
     *   even if other directories sort lexically before them 
     */
    if (atyp == 1)
    {
        if (a[0] == '.' && b[0] != '.')
            return -1;
        else if (a[0] != '.' && b[0] == '.')
            return 1;
    }

    /* they're of the same type, so sort lexically by the string */
    return stricmp(a, b);
}

/*
 *   This version is not applicable to Windows -- see oswinask.c 
 */
int os_askfile(const char *prompt, char *reply, int replen,
               int prompt_type, os_filetype_t file_type)
{
    static osfar_t char *curdir;           /* current directory we're using */
    static osfar_t int   curdirl;
    static osfar_t char  curdisk;               /* current disk we're using */
    static osfar_t int   gotdir;                    /* flag: we've read pwd */
    static osfar_t char *namebuf;    /* space for filenames (big enough???) */
    static osfar_t int   namebufl;
    int          old_keep_cursor_pos;
    int          rem;                         /* remaining space in namebuf */
    int          newfile;                   /* TRUE ==> creating a new file */
    char         lbuf[80];                                   /* line buffer */
    char        *p;
    char        *curpage;                 /* pointer to file at top of page */
    char        *curfile;                        /* pointer to current file */
    int          curofs;                  /* offset of current line on page */
    int          pageofs;                                 /* offset of page */
    int          filecnt;            /* number of files in the current list */
    int          ent_x, ent_y;            /* x/y location of filename entry */
    int          min_ent_x;                   /* first position of filename */
    char        *newp;            /* pointer to new filename entry location */
    int          insel;       /* TRUE: using selector; FALSE: entering name */
    int          lastofs;                     /* offset in list of lastfile */
    int          retval;
    int          bottom;
    int          instruct_y;
    int          list_top, list_bottom;
    int          top;
    int          left;
    int          right;
    int          y;
    int          i;
    static osfar_t char *lastfile;
    static osfar_t char  filebuf[256];
    char         runpwd[OSS_MAXPATH];
    char         rundisk;
    osdirhdl_t   dirhdl;
    unsigned long fmode;
    unsigned long fattr;
    char        *ext;
    size_t       extlen;
    unsigned long drives;
    static osfar_t char *ext_list[] =
    {
        ".GAM",                                                 /* OSFTGAME */
        ".SAV",                                                 /* OSFTSAVE */
        ".LOG",                                                  /* OSFTLOG */
        ".DAT",                                                 /* OSFTSWAP */
        ".DAT",                                                 /* OSFTDATA */
        ".CMD",                                                  /* OSFTCMD */
        ".MSG",                                                 /* OSFTERRS */
        ".TXT",                                                 /* OSFTTEXT */
        ".DAT",                                                  /* OSFTBIN */
        ".TCP",                                                 /* OSFTCMAP */
        ".DAT",                                                 /* OSFTPREF */
        "",                                                      /* OSFTUNK */
        ".t3", /* was t3x */                                   /* OSFTT3IMG */
        ".t3o",                                                /* OSFTT3OBJ */
        ".t3s",                                                /* OSFTT3SYM */
        ".t3v"                                                 /* OSFTT3SAV */
    };

    /* do any deferred redrawing */
    osssb_redraw_if_needed();

    /* allocate the buffers if we haven't already done so */
    if (namebuf == 0)
    {
        namebuf = (char *)calloc(namebufl = (256 * OSSFNAMELEN), 1);
        curdir = (char *)calloc(curdirl = OSS_MAXPATH, 1);
        lastfile = (char *)calloc(OSS_MAXPATH, 1);
    }

    /* simple version if in 'plain' mode */
    if (!namebuf || !curdir || !lastfile || os_f_plain || use_std_askfile)
    {
        /* show the prompt, and get a reply */
        os_printz(prompt);
        os_printz(" >");
        os_gets((uchar *)reply, replen);

        /* 
         *   return 'cancel' if they entered a blank line, otherwise
         *   success 
         */
        return (prompt[0] == '\0' ? OS_AFE_CANCEL : OS_AFE_SUCCESS);
    }
    
    /* we drive the cursor position from now on */
    old_keep_cursor_pos = S_keep_cursor_pos;
    S_keep_cursor_pos = TRUE;
    
    /* determine if we're writing or reading (different operations) */
    newfile = (prompt_type == OS_AFP_SAVE);
    insel = !newfile;

    /* 
     *   start with the most recently selected file, unless it was a
     *   directory or drive letter 
     */
    filebuf[0] = '\0';
    if (lastfile[0] != '\0')
    {
        char c = lastfile[strlen(lastfile) - 1];

        if (c != ':' && c != '\\')
            strcpy(filebuf, lastfile);
    }

    /* choose our extension filter based on the file type */
    if (file_type != OSFTUNK
        && (int)file_type >= 0
        && (int)file_type < sizeof(ext_list)/sizeof(ext_list[0]))
    {
        ext = ext_list[(int)file_type];
        extlen = strlen(ext);
    }
    else
    {
        ext = 0;
        extlen = 0;
    }

    /* remember the drive upon entry */
    rundisk = ossgetdisk();

    /* if we haven't set directory, read current one */
    if (!gotdir)
    {
        osspwd(curdir, curdirl);
        curdisk = ossgetdisk();
        gotdir = 1;
    }
    else
        osssetdisk(curdisk);
    
    /* 
     *   display the dialog box's outline
     */

    /* clear the box's screen area */
    top = 3;
    bottom = G_oss_screen_height - 4;
    left = 10;
    right = G_oss_screen_width - 11;
    ossclr(top, left, bottom, right, ldesc_color);

    /* display the line across the top */
    lbuf[0] = (char)218;
    for (p = lbuf+1, i = right - left - 1 ; i ; --i)
        *p++ = (uchar)196;
    *p++ = (char)191;
    *p++ = (char)0;
    ossdsp(top, left, ldesc_color, lbuf);

    /* figure out where the instructions go */
    instruct_y = bottom - 1;

    /* if we're creating a file, we need an extra two lines */
    if (newfile)
        instruct_y -= 2;

    /* 
     *   if we don't have drive letters available, we need a line for drive
     *   letter ALT-X-style selection instructions 
     */
    if (ossgetdisks() == 0)
        instruct_y -= 1;

    /* display the left and right side lines in the file list */
    lbuf[0] = (char)179;
    for (p = lbuf+1, i = right - left - 3 ; i ; --i)
        *p++ = ' ';
    *p++ = (char)179;
    *p++ = ' ';
    *p++ = (char)179;
    *p++ = (char)0;
    for (y = top+3 ; y < instruct_y - 1 ; ++y)
        ossdsp(y, left, ldesc_color, lbuf);

    /* add the lines in the current directory and instruction areas */
    *(p - 4) = ' ';
    ossdsp(top + 1, left, ldesc_color, lbuf);
    for (y = instruct_y ; y < bottom ; ++y)
        ossdsp(y, left, ldesc_color, lbuf);

    /* display the line under the current directory */
    lbuf[0] = (char)195;
    for (p = lbuf+1, i = right - left - 3 ; i ; --i)
        *p++ = (char)196;
    *p++ = (char)194;
    *p++ = (char)196;
    *p++ = (char)180;
    *p++ = (char)0;
    ossdsp(top+2, left, ldesc_color, lbuf);

    /* display the same line above the instructions */
    *(p - 4) = (char)193;
    ossdsp(instruct_y - 1, left, ldesc_color, lbuf);

    /* display the bottom line */
    lbuf[0] = (char)192;
    for (p = lbuf+1, i = right - left - 1 ; i ; --i)
        *p++ = (char)196;
    *p++ = (char)217;
    *p++ = (char)0;
    ossdsp(bottom, left, ldesc_color, lbuf);

    /* display the filename field if appropriate */
    y = instruct_y;
    if (newfile)
    {
        /* add the new filename field */
        ossdsp(y, left + 1, ldesc_color, "Filename:");

        /* note the location of the filename field */
        ent_y = y;
        ent_x = min_ent_x = left + 1 + 11;

        /* add a blank line after the filename field */
        y += 2;
    }

    /* add the main instructions */
    ossdsp(y, left + 1, ldesc_color,
           newfile
           ? "<Return>=OK, <Esc>=Cancel, <Tab>=List/Filename"
           : "<Return>=OK, <Esc>=Cancel");
    ++y;

    /* 
     *   add documentation on the ALT-X drive selection method, if we didn't
     *   add drive letters to the main list 
     */
    if (ossgetdisks() == 0)
        ossdsp(y, left + 1, ldesc_color,
               "<Alt+A>=go to drive A:, <Alt+B>=B:, etc.");

    /* calculate the file list's top and bottom lines */
    list_top = top + 3;
    list_bottom = instruct_y - 2;

reread_directory:
    /* save current directory and cd to save directory before reading */
    osspwd(runpwd, sizeof(runpwd));
    osscd(curdir);
    
    /* read the files in the directory matching the desired extension */
    p = namebuf;
    rem = namebufl - 2;             /* minus 2 to make room for extra nulls */
    filecnt = 0;
    lastofs = -1;             /* assume the last file is not found anywhere */
    if (os_open_dir(curdir, &dirhdl))
    {
        while (rem >= OSSFNAMELEN + 2 && os_read_dir(dirhdl, p, rem))
        {
            /* only include non-hidden files with the target extension */
            size_t len = strlen(p);
            if (osfmode(p, TRUE, &fmode, &fattr)
                && (fmode & OSFMODE_FILE) != 0
                && (fattr & (OSFATTR_HIDDEN | OSFATTR_SYSTEM)) == 0
                && (ext == 0
                    || (len >= extlen
                        && memicmp(p + len - extlen, ext, extlen) == 0)))
            {
                ++filecnt;
                rem -= /*l*/ OSSFNAMELEN;
                p += /*l*/ OSSFNAMELEN;
            }
        }
        os_close_dir(dirhdl);
    }

    /* now do another pass for just the subdirectories */
    if (os_open_dir(curdir, &dirhdl))
    {
        while (rem >= OSSFNAMELEN + 2 && os_read_dir(dirhdl, p, rem))
        {
            /* only include non-hidden directories, but omit '.' */
            if (osfmode(p, TRUE, &fmode, &fattr)
                && (fmode & OSFMODE_DIR) != 0
                && (fattr & (OSFATTR_HIDDEN | OSFATTR_SYSTEM)) == 0
                && strcmp(p, ".") != 0)
            {
                char *pend;
            
                ++filecnt;
                pend = p + strlen(p);
                *pend++ = '\\';
                *pend++ = '\0';
                rem -= /*l*/ OSSFNAMELEN;
                p += /*l*/ OSSFNAMELEN;
            }
        }
        os_close_dir(dirhdl);
    }

    /* add drive letters if possible */
    if ((drives = ossgetdisks()) != 0)
    {
        int bit;
        unsigned long mask;
        char drive_letter;

        /* scan for available drives */
        for (bit = 0, mask = 1, drive_letter = 'A' ; bit < 32 ;
             ++bit, ++drive_letter, mask <<= 1)
        {
            /* if this bit is set, add this drive to the list */
            if ((drives & mask) != 0 && rem >= OSSFNAMELEN + 2)
            {
                char *pend = p;

                /* add the drive letter and a colon */
                *pend++ = drive_letter;
                *pend++ = ':';
                *pend++ = '\0';
                rem -= OSSFNAMELEN;
                p += OSSFNAMELEN;

                /* count it */
                ++filecnt;
            }
        }
    }

    /* terminate the list with a pair of null bytes */
    *p++ = '\0';
    *p++ = '\0';
    
    /* go back to correct directory */
    osscd(runpwd);
    
    /* now sort the directory list */
    qsort(namebuf, (size_t)filecnt, (size_t)OSSFNAMELEN, fnamecmp);

    /* clear out interior of box (in case re-reading directory) */
    ossclr(list_top, left+1, list_bottom, right - 3, ldesc_color);

    /* clear out the current directory area */
    ossclr(top+1, left+1, top+1, right-1, ldesc_color);

    /* display current directory at top line */
    ossdsp(top+1, left+1, ldesc_color, curdir);
    
    /* set up entry area for new filename if creating a file */
    if (newfile)
    {
        /* clear out the area */
        ossclr(ent_y, ent_x, ent_y, right - 2, text_color);

        /* show the filename */
        ossdsp(ent_y, ent_x, text_color, filebuf);

        /* initialize the editing pointer */
        newp = filebuf;
    }
    
    /* look for previous file in list (so we can default it) */
    for (i = 0, p = namebuf ; i < filecnt ; p = fwd_file(p), ++i)
    {
        /* if this one matches the previous filename, note it */
        if (strcmp(lastfile, p) == 0)
            lastofs = i;
    }

    /* 
     *   if we didn't find a previous entry, initially select the first
     *   thing after the '..' entry, if it's present, or after the drive
     *   letter list, if they're present 
     */
    if (lastofs == -1)
    {
        /* scan the list */
        for (i = 0, p = namebuf ; i < filecnt ; p = fwd_file(p), ++i)
        {
            char c;
            
            /* 
             *   if this is a drive letter entry or the special parent
             *   directory "..", make sure we skip this entry, if there's a
             *   next entry 
             */
            c = p[strlen(p) - 1];
            if ((c == ':' || (p[0] == '.' && p[1] == '.' && p[2] == '\\'))
                && i + 1 < filecnt)
            {
                /* use the next entry */
                lastofs = i + 1;
            }
        }
    }
    
    /* default the previous file, if it is in the list */
    curpage = curfile = namebuf;
    curofs = 0;
    pageofs = 0;
    if (lastofs != -1)
    {
        for (i = 0 ; i < lastofs ; ++i)
        {
            curfile = fwd_file(curfile);
            if (curofs >= list_bottom - list_top)
            {
                ++pageofs;
                curpage = fwd_file(curpage);
            }
            else
                ++curofs;
        }
    }
    lastofs = -1;                    /* default the first time through only */
    
    /* display as many files as will fit */
    for (p = curpage, y = list_top ;
         *p != '\0' && y <= list_bottom && filecnt != 0 ; ++y)
    {
        ossdsp(y, left+2, ldesc_color, p);
        p = fwd_file(p);
    }
    
    /* read keyboard until we're done */
    for (;;)
    {
        unsigned char c;
        int      thumb_y;
        
        if (filecnt - (list_bottom - list_top + 1) > 0)
            thumb_y = (pageofs * (list_bottom - list_top)) /
                      (filecnt - (list_bottom - list_top));
        else
            thumb_y = 0;
        
        /* highlight current file, get char, unhighlight it */
        if (insel)
        {
            ossdsp(list_top + curofs, left + 2, text_color, curfile);
            ossdsp(list_top + thumb_y, right - 1, text_color, "\260");
            ossdsp(list_top + thumb_y + 1, right - 1, text_color, "\260");
        }

        /* locate cursor in entry area, if applicable */
        if (newfile && !insel)
            ossloc(ent_y, ent_x);
        else
            ossloc(list_top + curofs, left + 2);

        /* get the character */
        c = os_getc();

        if (insel)
        {
            ossdsp(list_top + curofs, left+2, ldesc_color, curfile);
            ossdsp(list_top + thumb_y, right - 1, ldesc_color, " ");
            ossdsp(list_top + thumb_y + 1, right - 1, ldesc_color, " ");
        }

        /* check for an extended key code */
        if (c == '\0')
        {
            /* it's an extended key - get the extended key code */
            c = os_getc();

            /* 
             *   check for drive-setting ALT keys (but only if we're using
             *   the ALT-X method to select drives, which we use only if we
             *   don't have a list of drive letters available in the main
             *   file list) 
             */
            if (drives == 0 && c >= CMD_ALT)
            {
                /* try setting drive, and continue if successful */
                osssetdisk(c - CMD_ALT);
                if (ossgetdisk() == c - CMD_ALT)
                {
                    curdisk = ossgetdisk();
                    osspwd(curdir, curdirl);
                    goto reread_directory;
                }
            }

            /* check what we have */
            switch(c)
            {
            case CMD_UP:
                /* if we're at the top of the list, we can't go up */
                if (curofs == 0 && pageofs == 0)
                    break;

                /* check where we are relative to the top of the screen */
                if (curofs == 0)
                {
                    /* top of the screen - scroll the list */
                    --pageofs;
                    curpage = backup_file(curpage, namebuf);
                    ossscu(list_top, left + 1, list_bottom, right - 3,
                           ldesc_color);
                }
                else
                {
                    /* move up a line */
                    --curofs;
                }

                /* find preceding null, then the one before that */
                curfile = backup_file(curfile, namebuf);
                break;
                
            case CMD_DOWN:
                if (!*fwd_file(curfile)) break;
                if (curofs >= list_bottom - list_top)
                {
                    ++pageofs;
                    curpage = fwd_file(curpage);
                    ossscr(list_top, left + 1,
                           list_bottom, right - 3, ldesc_color);
                }
                else
                    ++curofs;
                
                curfile = fwd_file(curfile);
                break;
                
            case CMD_KILL:
                /* cancel the dialog */
                retval = OS_AFE_CANCEL;
                goto done;

            case CMD_EOF:
                retval = OS_AFE_CANCEL;
                goto done;
                
            case CMD_TAB:
                if (!newfile) break;
                insel = !insel;
                break;
                
            default:
                break;
            }
        }
        else if (c == '\r')
        {
            /* select current entry - if it's a directory, open it */
            strcpy(reply, curfile);
            if (insel && strlen(reply) && reply[strlen(reply) - 1] == '\\')
            {
                char *root;
                
                /* set up correct directory for relative cd */
                osspwd(runpwd, sizeof(runpwd));
                osscd(curdir);

                /* get the root name of the current directory */
                root = os_get_root_name(curdir);

                /* add a trailing backslash to the current directory */
                if (strlen(curdir) && curdir[strlen(curdir)-1] != '\\')
                    strcat(curdir, "\\");
                reply[strlen(reply) - 1] = '\0';

                /*
                 *   If we're switching up to the parent directory, remember
                 *   the current directory as the last selected file - this
                 *   will initially select this directory in the parent list.
                 *   
                 *   If we're switching down to a subdirectory, remember
                 *   ".."  as the last selected file.  
                 */
                if (strcmp(curfile, "..\\") == 0)
                {
                    /* to parent - remember the current directory */
                    strcpy(lastfile, root);
                }
                else
                {
                    /* to child - remember the parent */
                    strcpy(lastfile, "..\\");
                }
                
                /* do relative cd and fetch new full path */
                osscd(reply);
                osspwd(curdir, curdirl);
                
                /* back to original directory, then reread directory */
                osscd(runpwd);
                goto reread_directory;
            }

            /* check to see if we're selecting a drive letter */
            if (insel && strlen(reply) && reply[strlen(reply) - 1] == ':')
            {
                /* try setting the drive */
                osssetdisk(reply[0] - 'A');
                if (ossgetdisk() == reply[0] - 'A')
                {
                    curdisk = ossgetdisk();
                    osspwd(curdir, curdirl);
                    goto reread_directory;
                }
            }

            /* if creating, and not in entry, copy file into entry */
            if (newfile && insel)
            {
                newp = filebuf;
                strcpy(filebuf, reply);
                ent_x = min_ent_x;
                insel = 0;
                ossclr(ent_y, ent_x, ent_y, right - 2, text_color);
                ossdsp(ent_y, ent_x, text_color, filebuf);
            }
            else
            {
                int l;
                    
                strcpy(reply, curdir);
                l = strlen(reply);
                if (!l || reply[l-1] != '\\') reply[l++] = '\\';
                strcpy(reply+l, newfile ? filebuf : curfile);
                retval = OS_AFE_SUCCESS;    /* successfully selected a file */
                strcpy(lastfile, filebuf);   /* remember file for next time */
                if (!strchr(lastfile, '.'))
                    strcat(lastfile, ext);
                if (!strchr(reply + l, '.'))
                    strcat(reply, ext);

                /* if the filename is empty, treat is as cancellation */
                if (!lastfile[0])
                    retval = OS_AFE_CANCEL;

                /* that's it */
                goto done;
            }
        }
        else if (c >= ' ' && c < 127
                 && !insel
                 && newp - filebuf < sizeof(filebuf) - 2
                 && newp - filebuf < replen - 2
                 && ent_x < right - 2)
        {
            /* ordinary character - clear any existing text in the field */
            if (*newp != '\0')
            {
                ossclr(ent_y, min_ent_x, ent_y, right - 2, text_color);
                *newp = '\0';
            }

            /* add the character */
            *newp++ = c;
            *newp = '\0';
            ossdsp(ent_y, ent_x, text_color, newp-1);
            ++ent_x;
        }
        else if (c == 8 && ent_x > min_ent_x)
        {
            /* backspace - delete the previous character */
            --ent_x;
            ossdsp(ent_y, ent_x, text_color, " ");
            *(--newp) = '\0';
        }
        else if (c == 9 && newfile)
        {
            /* tab - switch to the other field */
            insel = !insel;
        }
    }
    
done:
    /* cd back to entry disk */
    osssetdisk(rundisk);

    /* now erase the file selector, redraw, and we're done */
    ossclr(top, left, bottom, right, ldesc_color);
    os_redraw();

    /* we're done with the special cursor position control */
    S_keep_cursor_pos = old_keep_cursor_pos;
        
    /* return the result */
    return retval;
}

#  endif /* !WINDOWS */
# endif /* RUNTIME */
#endif /* USEDOSCOMMON */

/*
 *   set the title 
 */
void os_set_title(const char *ttl)
{
    /* do nothing in this implementation */
}

/*
   Check for a game to restore given as a startup parameter.
   Returns TRUE if a game is to be restored, FALSE otherwise.
   The DOS implementation of this function always returns FALSE,
   since there's no way to specify a saved game at the DOS
   command line.
*/
int os_paramfile(char *buf)
{
    return(0);
}

#ifndef __DPMI32__
#ifndef WINDOWS
#ifndef T_WIN32
# ifdef MSOS2                            /* ### Look at this later. [Eras] */
int os_break() { return(0); }
void os_instbrk(int install) {}
# else /* !MSOS2 */
/*
 *   Check for control-break.  Returns status of break flag, and clears
 *   the flag. 
 */
static osfar_t int break_set;
int os_break(void)
{
    int ret = break_set;
    break_set = 0;
    return(ret);
}

#  ifdef DJGPP

/* break handler */
static void break_handler(int sig)
{
    break_set = 1;
}

/* install or remove control-break handler */
void os_instbrk(int install)
{
    if (install)
        signal(SIGINT, break_handler);
    else
        signal(SIGINT, SIG_DFL);
}

#  else /* DJGPP */

/* break handler */
static interrupt void break_handler(void)
{
    break_set = 1;
}

/* install or remove control-break handler */
void os_instbrk(int install)
{
    static void (interrupt *old)();

    if (install)
    {
        old = getvect((unsigned)0x1b);
        setvect(0x1b, break_handler);
    }
    else
        setvect(0x1b, old);
}
#  endif /* DJGPP */
# endif /* MSOS2 */
#endif /* T_WIN32 */
#endif /* !WINDOWS */
#endif /* __DPMI32__ */


#ifdef __DPMI32__
int os_break()
{
    return 0;
}
#endif /* __DPMI32__ */

/* ------------------------------------------------------------------------ */
#ifndef WINDOWS
/*
 *   HTML 4 character set translation.  This translates to the DOS console
 *   character set.  This is only appropriate in DOS character mode, since
 *   Windows uses a different character set.  
 */
void os_xlat_html4(unsigned int html4_char, char *result, size_t result_len)
{
    /* default character to use for unknown charaters */
#define INV_CHAR "\372"

    /* 
     *   Translation table - provides mappings for characters the ISO
     *   Latin-1 subset of the HTML 4 character map (values 128-255).
     *   
     *   Characters marked "(approx)" are approximations where the actual
     *   desired character is not available in the DOS console character
     *   set, but a reasonable approximation is used.  Characters marked
     *   "(approx unaccented)" are accented characters that are not
     *   available; these use the unaccented equivalent as an
     *   approximation, since this will presumably convey more meaning
     *   than a blank.
     *   
     *   Characters marked "(n/a)" have no equivalent (even approximating)
     *   in the DOS character set, and are mapped to spaces.
     *   
     *   Characters marked "(not used)" are not used by HTML '&' markups.  
     */
    static osfar_t const char *xlat_tbl[] =
    {
        INV_CHAR,                                         /* 128 (not used) */
        INV_CHAR,                                         /* 129 (not used) */
        "'",                                        /* 130 - sbquo (approx) */
        INV_CHAR,                                         /* 131 (not used) */
        "\"",                                       /* 132 - bdquo (approx) */
        INV_CHAR,                                         /* 133 (not used) */
        INV_CHAR,                                     /* 134 - dagger (n/a) */
        INV_CHAR,                                     /* 135 - Dagger (n/a) */
        INV_CHAR,                                         /* 136 (not used) */
        INV_CHAR,                                     /* 137 - permil (n/a) */
        INV_CHAR,                                         /* 138 (not used) */
        "<",                                       /* 139 - lsaquo (approx) */
        INV_CHAR,                                      /* 140 - OElig (n/a) */
        INV_CHAR,                                         /* 141 (not used) */
        INV_CHAR,                                         /* 142 (not used) */
        INV_CHAR,                                         /* 143 (not used) */
        INV_CHAR,                                         /* 144 (not used) */
        "'",                                        /* 145 - lsquo (approx) */
        "'",                                        /* 146 - rsquo (approx) */
        "\"",                                       /* 147 - ldquo (approx) */
        "\"",                                       /* 148 - rdquo (approx) */
        INV_CHAR,                                         /* 149 (not used) */
        "\304",                                             /* 150 - endash */
        "--",                                               /* 151 - emdash */
        INV_CHAR,                                         /* 152 (not used) */
        "(tm)",                                     /* 153 - trade (approx) */
        INV_CHAR,                                         /* 154 (not used) */
        ">",                                       /* 155 - rsaquo (approx) */
        INV_CHAR,                                      /* 156 - oelig (n/a) */
        INV_CHAR,                                         /* 157 (not used) */
        INV_CHAR,                                         /* 158 (not used) */
        "Y",                              /* 159 - Yuml (approx unaccented) */
        INV_CHAR,                                         /* 160 (not used) */
        "\255",                                              /* 161 - iexcl */
        "\233",                                               /* 162 - cent */
        "\234",                                              /* 163 - pound */
        INV_CHAR,                                     /* 164 - curren (n/a) */
        "\235",                                                /* 165 - yen */
        "\263",                                             /* 166 - brvbar */
        INV_CHAR,                                       /* 167 - sect (n/a) */
        INV_CHAR,                                        /* 168 - uml (n/a) */
        "(c)",                                       /* 169 - copy (approx) */
        "\246",                                               /* 170 - ordf */
        "\256",                                              /* 171 - laquo */
        "\252",                                                /* 172 - not */
        " ",                                             /* 173 - shy (n/a) */
        "(R)",                                        /* 174 - reg (approx) */
        INV_CHAR,                                       /* 175 - macr (n/a) */
        "\370",                                                /* 176 - deg */
        "\361",                                             /* 177 - plusmn */
        "\375",                                               /* 178 - sup2 */
        INV_CHAR,                                       /* 179 - sup3 (n/a) */
        "'",                                        /* 180 - acute (approx) */
        "\346",                                              /* 181 - micro */
        INV_CHAR,                                       /* 182 - para (n/a) */
        "\371",                                             /* 183 - middot */
        ",",                                        /* 184 - cedil (approx) */
        INV_CHAR,                                       /* 185 - sup1 (n/a) */
        "\247",                                               /* 186 - ordm */
        "\257",                                              /* 187 - raquo */
        "\254",                                             /* 188 - frac14 */
        "\253",                                             /* 189 - frac12 */
        "3/4",                                     /* 190 - frac34 (approx) */
        "\250",                                             /* 191 - iquest */
        "A",                            /* 192 - Agrave (approx unaccented) */
        "A",                            /* 193 - Aacute (approx unaccented) */
        "A",                             /* 194 - Acirc (approx unaccented) */
        "A",                            /* 195 - Atilde (approx unaccented) */
        "\216",                                               /* 196 - Auml */
        "\217",                                              /* 197 - Aring */
        "\222",                                              /* 198 - AElig */
        "\200",                                             /* 199 - Ccedil */
        "E",                            /* 200 - Egrave (approx unaccented) */
        "\220",                                             /* 201 - Eacute */
        "E",                             /* 202 - Ecirc (approx unaccented) */
        "E",                              /* 203 - Euml (approx unaccented) */
        "I",                            /* 204 - Igrave (approx unaccented) */
        "I",                            /* 205 - Iacute (approx unaccented) */
        "I",                             /* 206 - Icirc (approx unaccented) */
        "I",                              /* 207 - Iuml (approx unaccented) */
        INV_CHAR,                                        /* 208 - ETH (n/a) */
        "\245",                                             /* 209 - Ntilde */
        "O",                            /* 210 - Ograve (approx unaccented) */
        "O",                            /* 211 - Oacute (approx unaccented) */
        "O",                             /* 212 - Ocirc (approx unaccented) */
        "O",                            /* 213 - Otilde (approx unaccented) */
        "\231",                                               /* 214 - Ouml */
        "x",                                        /* 215 - times (approx) */
        "O",                            /* 216 - Oslash (approx unaccented) */
        "U",                            /* 217 - Ugrave (approx unaccented) */
        "U",                            /* 218 - Uacute (approx unaccented) */
        "U",                             /* 219 - Ucirc (approx unaccented) */
        "\232",                                               /* 220 - Uuml */
        "Y",                            /* 221 - Yacute (approx unaccented) */
        INV_CHAR,                                      /* 222 - THORN (n/a) */
        "\341",                                     /* 223 - szlig (approx) */
        "\205",                                             /* 224 - agrave */
        "\240",                                             /* 225 - aacute */
        "\203",                                              /* 226 - acirc */
        "a",                            /* 227 - atilde (approx unaccented) */
        "\204",                                               /* 228 - auml */
        "\206",                                              /* 229 - aring */
        "\221",                                              /* 230 - aelig */
        "\207",                                             /* 231 - ccedil */
        "\212",                                             /* 232 - egrave */
        "\202",                                             /* 233 - eacute */
        "\210",                                              /* 234 - ecirc */
        "\211",                                               /* 235 - euml */
        "\215",                                             /* 236 - igrave */
        "\241",                                             /* 237 - iacute */
        "\214",                                              /* 238 - icirc */
        "\213",                                               /* 239 - iuml */
        INV_CHAR,                                        /* 240 - eth (n/a) */
        "\244",                                             /* 241 - ntilde */
        "\225",                                             /* 242 - ograve */
        "\242",                                             /* 243 - oacute */
        "\223",                                              /* 244 - ocirc */
        "o",                            /* 245 - otilde (approx unaccented) */
        "\224",                                               /* 246 - ouml */
        "\366",                                             /* 247 - divide */
        "o",                            /* 248 - oslash (approx unaccented) */
        "\227",                                             /* 249 - ugrave */
        "\243",                                             /* 250 - uacute */
        "\226",                                              /* 251 - ucirc */
        "\201",                                               /* 252 - uuml */
        "y",                            /* 253 - yacute (approx unaccented) */
        INV_CHAR,                                      /* 254 - thorn (n/a) */
        "\230"                                                /* 255 - yuml */
    };

    /*
     *   Map certain extended characters into our 128-255 range.  If we
     *   don't recognize the character, return the default invalid
     *   charater value.  
     */
    if (html4_char > 255)
    {
        switch(html4_char)
        {
        case 338: html4_char = 140; break;
        case 339: html4_char = 156; break;
        case 376: html4_char = 159; break;
        case 352: html4_char = 154; break;
        case 353: html4_char = 138; break;
        case 8211: html4_char = 150; break;
        case 8212: html4_char = 151; break;
        case 8216: html4_char = 145; break;
        case 8217: html4_char = 146; break;
        case 8218: html4_char = 130; break;
        case 8220: html4_char = 147; break;
        case 8221: html4_char = 148; break;
        case 8222: html4_char = 132; break;
        case 8224: html4_char = 134; break;
        case 8225: html4_char = 135; break;
        case 8240: html4_char = 137; break;
        case 8249: html4_char = 139; break;
        case 8250: html4_char = 155; break;
        case 8482: html4_char = 153; break;

        default:
            /* unmappable character - return mid-dot character */
            result[0] = (unsigned char)250;
            result[1] = 0;
            return;
        }
    }
    
    /* 
     *   if the character is in the regular ASCII zone, return it
     *   untranslated 
     */
    if (html4_char < 128)
    {
        result[0] = (unsigned char)html4_char;
        result[1] = '\0';
        return;
    }

    /* look up the character in our table and return the translation */
    strcpy(result, xlat_tbl[html4_char - 128]);
}

#endif /* WINDOWS */

#ifdef DJGPP
#if DJGPP < 2 || (DJGPP == 2 && DJGPP_MINOR <= 1)

/*
 *   Provide memicmp, since it's not a standard libc routine.  
 */
memicmp(char *s1, char *s2, int len)
{
    int i;

    for (i = 0; i < len; i++)
    {
        if (tolower(s1[i]) != tolower(s2[i]))
            return (int)tolower(s1[i]) - (int)tolower(s2[i]);
    }
    return 0;
}

#endif
#endif /* DJGPP */

/*
 *   Use the system strlwr() implementation for os_strlwr() 
 */
char *os_strlwr(char *s)
{
    return strlwr(s);
}

/* ------------------------------------------------------------------------ */
/*
 *   Character mapping functions 
 */

#ifdef T_WIN32
/* ------------------------------------------------------------------------ */
/*
 *   Get the DOS codepage.  For the 32-bit console app version, call the
 *   console function.  
 */
static int oss_get_dos_codepage(void)
{
    return GetConsoleOutputCP();
}

/*
 *   Get the filename character mapping for Unicode characters.
 *   
 *   - For the display, we use the OEM mapping, since we're running under a
 *   DOS console, and DOS consoles use the OEM character set
 *   
 *   - For filenames, we use the ANSI mapping, since file operations are in
 *   the ANSI character set
 *   
 *   - For file contents, we use the ANSI mapping, since most files will have
 *   been created by Windows apps that use the Windows character set 
 */
void os_get_charmap(char *mapname, int charmap_id)
{
    int cp;
    HANDLE h;

    /* get the correct code page for the usage */
    switch(charmap_id)
    {
    case OS_CHARMAP_DISPLAY:
        /* 
         *   Try opening the console output handle.  This will let us
         *   determine if we're running under a DOS box or under a different
         *   shell, such as Workbench.  If we're running under a DOS box,
         *   we'll use the OEM code page for console output; otherwise, we'll
         *   assume we're running in a graphical shell, so we'll use the ANSI
         *   code page.  
         */
        if ((h = CreateFile("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE,
                            0, OPEN_EXISTING, 0, 0)) != INVALID_HANDLE_VALUE)
        {
            /* we have a console (aka DOS box) - use the OEM code page */
            cp = GetOEMCP();
            CloseHandle(h);
        }
        else
        {
            /* 
             *   no console - assume we're running in a graphic shell, so use
             *   the ANSI code page 
             */
            cp = GetACP();
        }
        break;

    case OS_CHARMAP_CMDLINE:
        /* 
         *   even though CMD.EXE uses the OEM code page for display, it seems
         *   to pass argv strings in the ANSI code page (or this could be an
         *   MSVC feature, but it's what we get in any case)
         */
        cp = GetACP();
        break;

    case OS_CHARMAP_FILENAME:
    case OS_CHARMAP_FILECONTENTS:
        /* use the ANSI character set for filenames and contents */
        cp = GetACP();
        break;

    default:
        /* for anything unknown, use the ANSI character set */
        cp = GetACP();
        break;
    }

    /* build the name as "CPnnnn", where nnnn is the ANSI code page */
    sprintf(mapname, "CP%d", cp);
}

#else /* T_WIN32 */

/*
 *   Get the current DOS codepage.  This is the plain 16-bit DOS version - we
 *   simply call the int 21h function.  
 */
static int oss_get_dos_codepage(void)
{
    int codepage;

#ifdef DJGPP
    __dpmi_regs regs;

    regs.x.ax = 0x6601;
    __dpmi_int(0x21, &regs);
    codepage = regs.x.bx;

#else /* DJGPP */
    __asm {
        mov   ax, 6601h
        int   21h
        mov   [codepage], bx
    }
#endif /* DJGPP */
    return codepage;
}

/*
 *   Get the character mapping for Unicode characters.  We use the current
 *   DOS code page for all character maps.  
 */
void os_get_charmap(char *mapname, int charmap_id)
{
    int codepage;

    /* get the current DOS code page */
    codepage = oss_get_dos_codepage();

    /* build the name as "CPnnnn", where nnnn is the code page number */
    sprintf(mapname, "CP%d", codepage);
}

#endif /* T_WIN32 */


/* ------------------------------------------------------------------------ */
/*
 *   Generate a filename for a character mapping table.  We will generate a
 *   name starting with the current DOS code page identifier, then the
 *   internal ID, and the suffix ".TCP".  The DOS code page ID and the
 *   internal ID will both be four characters at most, so the name will
 *   always fit in the DOS 8.3 filename limit.  
 */
void os_gen_charmap_filename(char *filename, char *internal_id,
                             char *argv0)
{
    char *p;
    char *rootname;
    size_t pathlen;
    int  codepage;

    /* get the current DOS code page */
    codepage = oss_get_dos_codepage();

    /* find the path prefix of the original executable filename */
    for (p = rootname = argv0 ; *p != '\0' ; ++p)
    {
        if (*p == '/' || *p == '\\' || *p == ':')
            rootname = p + 1;
    }

    /* copy the path prefix */
    pathlen = rootname - argv0;
    memcpy(filename, argv0, pathlen);

    /* if there's no trailing backslash, add one */
    if (pathlen == 0 || filename[pathlen - 1] != '\\')
        filename[pathlen++] = '\\';

    /* add the code page ID, the internal ID, and the extension */
    sprintf(filename + pathlen, "%u", codepage);
    strcat(filename + pathlen, internal_id);
    strcat(filename + pathlen, ".tcp");
}

/*
 *   Receive notification that a character mapping file has been loaded.  We
 *   don't need to do anything with this information, since we require the
 *   player to set an appropriate code page prior to starting the run-time.  
 */
void os_advise_load_charmap(char *id, char *ldesc, char *sysinfo)
{
    /* there's nothing we need to do here */
}

/*
 *   Translate keystrokes from "raw" to "processed" mode.  This translates
 *   keystrokes from the raw representation used by os_getc_raw() to the
 *   editing commands used in os_getc(). 
 */
void oss_raw_key_to_cmd(os_event_info_t *evt)
{
    /* check the first character of the keystroke event */
    switch(evt->key[0])
    {
    case 0:
        /* it's a special key - check the special key code */
        switch(evt->key[1])
        {
        case CMD_F1:
            /* toggle scrollback mode */
            evt->key[1] = CMD_SCR;
            break;
        }
        break;

    case 1:
        /* ^A - start of line */
        evt->key[0] = 0;
        evt->key[1] = CMD_HOME;
        break;

    case 2:
        /* ^B - left */
        evt->key[0] = 0;
        evt->key[1] = CMD_LEFT;
        break;

    case 4:
        /* ^D - delete */
        evt->key[0] = 0;
        evt->key[1] = CMD_DEL;
        break;

    case 5:
        /* ^E - end of line */
        evt->key[0] = 0;
        evt->key[1] = CMD_END;
        break;
        
    case 6:
        /* ^F - right */
        evt->key[0] = 0;
        evt->key[1] = CMD_RIGHT;
        break;

    case 11:
        /* ^K - delete to end of line */
        evt->key[0] = 0;
        evt->key[1] = CMD_DEOL;
        break;

    case 21:
    case 27:
        /* ^U/Escape - delete entire line */
        evt->key[0] = 0;
        evt->key[1] = CMD_KILL;
        break;
    }
}

#ifndef DJGPP
#ifdef __TURBOC__
# define _eof(handle) eof(handle)
# define _fileno(stream) fileno(stream)
#endif

/*
 *   Is stdin at EOF?  Note that we need to check the low-level OS file
 *   handle, not the stdio stream handle - the stream handle doesn't appear
 *   to notice that it's at EOF until an attempt has been made to read from
 *   the stream, whereas the low-level OS handle knows without reading.  
 */
int oss_eof_on_stdin()
{
    return _eof(_fileno(stdin)) > 0;
}
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Default implementation of sprintf equivalents with memory allocation.
 *   The old Borland library doesn't have a version of sprintf with buffer
 *   limits, nor does DJGPP, so we take a somewhat cheesy but workable
 *   approach: we format to the NUL: device to do the length counting.  
 */
#if defined(TURBO) || defined(DJGPP)
int os_asprintf(char **bufptr, const char *fmt, ...)
{
    va_list argp;
    int ret;

    va_start(argp, fmt);
    ret = os_vasprintf(bufptr, fmt, argp);
    va_end(argp);

    return ret;
}

int os_vasprintf(char **bufptr, const char *fmt, va_list argp)
{
    int len;
    FILE *fp;

    /* open the NUL: device so that we can do our length counting */
    fp = fopen("NUL", "w");
    if (fp == 0)
        return -1;

    /* format the string to NUL: */
    len = fprintf(fp, fmt, argp);

    /* done with the file */
    fclose(fp);

    /* if the fprintf failed, return failure */
    if (len < 0)
        return -1;

    /* allocate a buffer */
    if ((*bufptr = (char *)osmalloc(len + 1)) == 0)
        return -1;

    /* format the result */
    return vsprintf(*bufptr, fmt, argp);
}
#endif /* TURBO */

