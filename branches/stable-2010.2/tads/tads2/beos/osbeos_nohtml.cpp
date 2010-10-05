/* 
 *   osbeos.c -- copyright (c) 2000 by Christopher Tate, all rights reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */

// Just the parts of osbeos.cpp that are superceded by HTML Tads

// appctx.h needs size_t to be defined
#include <size_t.h>
#include "appctx.h"
#include "os.h"

extern "C" {

void set_cbreak_mode (int mode);

/* write a string to the main console */
void os_printz(const char *str)
{
    fputs(str, stdout);
}

/* write a non-null-terminated string to the console */
void os_print(const char *str, size_t len)
{
        if (os_get_status() == 0)               // discard non-main-window
        {
            printf("%.*s", (int)len, str);
        }
}

/* display a string in the score area in the status line.  These stubs are only for non-HTML tads */
void os_strsc(const char* /*p*/)
{
        // no status line; ignore
}

/* set the status line mode */
// global state:  are we updating the status line, or the main text of the game log?
static int gStatusMode = 0;

void os_status(int stat)
{
        gStatusMode = stat;
}

/* get the status line mode */
int os_get_status()
{
        return gStatusMode;
}

/* set the score value */
void os_score(int /*cur*/, int /*turncount*/)
{
        // no status line; ignore
}

/* clear the screen */
void oscls(void)
{
        // !!! don't do anything for now
}

/* flush any buffered display output */
void os_flush(void)
{
        fflush(stdout);
}

/*
 *   Get highlighting character code.  This returns a character code to
 *   write to the display in order to activate a highlighting mode: mode 1
 *   = turn on boldface, mode 2 = turn off boldface.  A return value of
 *   zero indicates that highlighting is not supported; otherwise, the
 *   return value is the character to write to the display to activate the
 *   given mode.  
 */
int os_hilite(int /*mode*/)
{
        return 0;               // !!! no hilighting for now
}

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
#endif  // #ifndef os_plain

/*
 *   Set the game title.  The output layer calls this routine when a game
 *   sets its title (via an HTML <title> tag, for example).  If it's
 *   convenient to do so, the OS layer can use this string to set a window
 *   caption, or whatever else makes sense on each system.  Most
 *   character-mode implementations will provide an empty implementation,
 *   since there's not usually any standard way to show the current
 *   application title on a character-mode display.  
 */
void os_set_title(const char */*title*/)
{
        // don't bother on BeOS in a Terminal
}

/* These terminal-based input functions are only for non-HTML tads */
unsigned char *os_gets(unsigned char *buf, size_t bufl)
{
        fgets((char*) buf, bufl, stdin);

        // postprocess the string to handle editing characters, etc.
        char* newbuf = (char*) malloc(bufl);
        memcpy(newbuf, buf, bufl);

        char* p = (char*) buf;
        char* q = newbuf;
        while (*p)
        {
                switch (*p)
                {
                case 0x08:              // backspace
                        if (q > newbuf) q--;
                        break;

                case 0x15:              // control-U == kill entire line
                        q = newbuf;
                        break;

                case '\r':                      // end of line
                case '\n':
                        *p = *q = '\0';
                        break;

                default:
                        *q++ = *p++;
                        break;
                }
        }

        memcpy(buf, newbuf, bufl);
        free(newbuf);
        return buf;
}

/* 
 *   Read a character from the keyboard.  For extended keystrokes, this
 *   function returns zero, and then returns the CMD_xxx code for the
 *   extended keystroke on the next call.  For example, if the user
 *   presses the up-arrow key, the first call to os_getc() should return
 *   0, and the next call should return CMD_UP.  Refer to the CMD_xxx
 *   codes below.
 *   
 *   os_getc() should return a high-level, translated command code for
 *   command editing.  This means that, where a functional interpretation
 *   of a key and the raw key-cap interpretation both exist as CMD_xxx
 *   codes, the functional interpretation should be returned.  For
 *   example, on Unix, Ctrl-E is conventionally used in command editing to
 *   move to the end of the line, following Emacs key bindings.  Hence,
 *   os_getc() should return CMD_END for this keystroke, rather than
 *   (CMD_CTRL + 'E' - 'A'), because CMD_END is the high-level command
 *   code for the operation.
 *   
 *   The translation ability of this function allows for system-dependent
 *   key mappings to functional meanings.  
 */
int os_getc(void)
{
        return os_getc_raw();
}

/*
 *   Read a character from the keyboard, following the same protocol as
 *   os_getc() for CMD_xxx codes (i.e., when an extended keystroke is
 *   encountered, os_getc_raw() returns zero, then returns the CMD_xxx
 *   code on the subsequent call).
 *   
 *   This function differs from os_getc() in that this function returns
 *   the low-level, untranslated key code.  This means that, when a
 *   functional interpretation of a key and the raw key-cap interpretation
 *   both exist as CMD_xxx codes, this function returns the key-cap
 *   interpretation.  For the Unix Ctrl-E example in the comments
 *   describing os_getc() above, this function should return 5 (the ASCII
 *   code for Ctrl-E), because the CMD_CTRL interpretation is the
 *   low-level key code.
 *   
 *   This function should return all control keys using their ASCII
 *   control codes, whenever possible.  Similarly, this function should
 *   return ASCII 27 for the Escape key, if possible.  
 */
int os_getc_raw(void)
{
        set_cbreak_mode(1);
        int c = getchar();
        set_cbreak_mode(0);
        return c;
}

/* wait for a character to become available from the keyboard */
void os_waitc(void)
{
        (void) os_getc();
}

/*
 *   Get an input event.  The event types are shown above.  If use_timeout
 *   is false, this routine should simply wait until one of the events it
 *   recognizes occurs, then return the appropriate information on the
 *   event.  If use_timeout is true, this routine should return
 *   OS_EVT_TIMEOUT after the given number of milliseconds elapses if no
 *   event occurs first.
 *   
 *   This function is not obligated to obey the timeout.  If a timeout is
 *   specified and it is not possible to obey the timeout, the function
 *   should simply return OS_EVT_NOTIMEOUT.  The trivial implementation
 *   thus checks for a timeout, returns an error if specified, and
 *   otherwise simply waits for the user to press a key.  
 */
int os_get_event(unsigned long /*timeout_in_milliseconds*/, int use_timeout, os_event_info_t *info)
{
    /* if there's a timeout, return an error indicating we don't allow it */
    if (use_timeout)
        return OS_EVT_NOTIMEOUT;

    /* get a key the normal way */
    info->key[0] = os_getc();

    /* if it's an extended key, get the other key */
    if (info->key[0] == 0)
        info->key[1] = os_getc();

    /* return the keyboard event */
    return OS_EVT_KEY;
}

/* pause prior to exit */
void os_expause(void)
{
    os_printz("(Strike any key to exit...)");
    os_flush();
    os_waitc();
    os_printz("\n");
}

/*
 *   Check for user break (control-C, etc) - returns true if a break is
 *   pending, false if not.  If this returns true, it should "consume" the
 *   pending break (probably by simply clearing the OS code's internal
 *   break-pending flag).  
 */
int os_break(void)
{
        return false;
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
void os_xlat_html4(unsigned int html4_char,
                   char *result, size_t result_buf_len)
{
        // This routine actually translates a Unicode-16 character to an appropriate
        // implementation-defined string.  On BeOS, this just means translating it to
        // UTF-8, which is supported by the sprintf() routine's "wide char" formatting.
        snprintf(result, result_buf_len, "%lc", html4_char);
}
# endif
#endif

} // extern "C"
