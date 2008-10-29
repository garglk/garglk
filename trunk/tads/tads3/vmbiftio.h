/* $Header: d:/cvsroot/tads/tads3/vmbiftad.h,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbiftad.h - function set definition - TADS I/O function set
Function

Notes

Modified
  12/06/98 MJRoberts  - Creation
*/

#ifndef VMBIFTIO_H
#define VMBIFTIO_H

#include "os.h"
#include "vmbif.h"
#include "utf8.h"

/* ------------------------------------------------------------------------ */
/*
 *   TADS I/O function set built-in functions 
 */
class CVmBifTIO: public CVmBif
{
public:
    /*
     *   Input/output functions 
     */
    static void say(VMG_ uint argc);
    static void logging(VMG_ uint argc);
    static void clearscreen(VMG_ uint argc);
    static void more(VMG_ uint argc);
    static void input(VMG_ uint argc);
    static void inputkey(VMG_ uint argc);
    static void inputevent(VMG_ uint argc);
    static void inputdialog(VMG_ uint argc);
    static void askfile(VMG_ uint argc);
    static void timedelay(VMG_ uint argc);
    static void sysinfo(VMG_ uint argc);
    static void status_mode(VMG_ uint argc);
    static void status_right(VMG_ uint argc);
    static void res_exists(VMG_ uint argc);
    static void set_script_file(VMG_ uint argc);
    static void get_charset(VMG_ uint argc);
    static void flush_output(VMG_ uint argc);
    static void input_timeout(VMG_ uint argc);
    static void input_cancel(VMG_ uint argc);

    /*
     *   banner window functions 
     */
    static void banner_create(VMG_ uint argc);
    static void banner_delete(VMG_ uint argc);
    static void banner_say(VMG_ uint argc);
    static void banner_clear(VMG_ uint argc);
    static void banner_flush(VMG_ uint argc);
    static void banner_size_to_contents(VMG_ uint argc);
    static void banner_goto(VMG_ uint argc);
    static void banner_set_text_color(VMG_ uint argc);
    static void banner_set_screen_color(VMG_ uint argc);
    static void banner_get_info(VMG_ uint argc);
    static void banner_set_size(VMG_ uint argc);

    /*
     *   log console functions 
     */
    static void log_console_create(VMG_ uint argc);
    static void log_console_close(VMG_ uint argc);
    static void log_console_say(VMG_ uint argc);

protected:
    /*
     *   Map an "extended" keystroke from raw (os_getc_raw, os_get_event) to
     *   portable UTF-8 representation.  An extended keystroke is one of the
     *   CMD_xxx key sequences, which the os_xxx layer represents by
     *   returning a null (zero) byte followed by a CMD_xxx code.  The caller
     *   should simply pass the second byte of the sequence here, and we'll
     *   provide a suitable key name.  The buffer that 'buf' points to must
     *   be at least 32 bytes long.  
     *   
     *   - For ALT keys, we'll return a name like '[alt-x]'.
     *   
     *   - For CMD_xxx codes, we'll return a key name enclosed in square
     *   brackets; for example, for a left cursor arrow key, we'll return
     *   '[left]'.
     *   
     *   - For unknown CMD_xxx keys, we'll return '[?]'.  
     */
    static int map_ext_key(VMG_ char *buf, int extc);

    /* 
     *   Map a keystroke from raw (os_getc_raw) notation to a portable UTF-8
     *   representation.  Takes an array of bytes giving the local character,
     *   which might be represented as a multi-byte sequence.  The caller is
     *   responsible for calling raw_key_complete() to determine when enough
     *   bytes have been fetched from the osifc layer to form a complete
     *   character.
     *   
     *   - For backspace (ctrl-H or ASCII 127), we'll return '[bksp]'.
     *   
     *   - For ASCII 10 or 13, we'll return ASCII 10 ('\n')
     *   
     *   - For ASCII 9, we'll return ASCII 9 ('\t').
     *   
     *   - For other control characters, we'll return a name like '[ctrl-z]'.
     *   
     *   - For Escape, we'll return '[esc]'.
     *   
     *   - For any non-control character that isn't a CMD_xxx command code,
     *   we'll map the character to UTF-8 and return the resulting single
     *   character (which might, of course, be more than one byte long).
     *   
     *   Returns true if we found a valid mapping for the key, false if not
     *   (in which case the buffer will be filled in with '[?]'.  The return
     *   buffer is always null-terminated.  
     */
    static int map_raw_key(VMG_ char *buf, const char *c, size_t len);

    /*
     *   Determine if the given byte sequence constitutes a complete
     *   character in the local character set.  Returns true if so, false if
     *   not.  Callers can use this to determine how many bytes must be
     *   fetched from the keyboard to complete an entire character sequence
     *   in the local character set, which can be important when using a
     *   multi-byte character set.  
     */
    static int raw_key_complete(VMG_ const char *c, size_t len);

    /* display one or more arguments to a given console */
    static void say_to_console(VMG_ class CVmConsole *console, uint argc);

    /* close the current script file */
    static void close_script_file(VMG0_);
};


#endif /* VMBIFTIO_H */

/* ------------------------------------------------------------------------ */
/*
 *   TADS I/O function set vector.  Define this only if VMBIF_DEFINE_VECTOR
 *   has been defined, so that this file can be included for the prototypes
 *   alone without defining the function vector.
 *   
 *   Note that this vector is specifically defined outside of the section of
 *   the file protected against multiple inclusion.  
 */
#ifdef VMBIF_DEFINE_VECTOR

/* TADS input/output functions */
void (*G_bif_tadsio[])(VMG_ uint) =
{
    &CVmBifTIO::say,
    &CVmBifTIO::logging,
    &CVmBifTIO::clearscreen,
    &CVmBifTIO::more,
    &CVmBifTIO::input,
    &CVmBifTIO::inputkey,
    &CVmBifTIO::inputevent,
    &CVmBifTIO::inputdialog,
    &CVmBifTIO::askfile,
    &CVmBifTIO::timedelay,
    &CVmBifTIO::sysinfo,
    &CVmBifTIO::status_mode,
    &CVmBifTIO::status_right,
    &CVmBifTIO::res_exists,
    &CVmBifTIO::set_script_file,
    &CVmBifTIO::get_charset,
    &CVmBifTIO::flush_output,
    &CVmBifTIO::input_timeout,
    &CVmBifTIO::input_cancel,

    &CVmBifTIO::banner_create,
    &CVmBifTIO::banner_delete,
    &CVmBifTIO::banner_clear,
    &CVmBifTIO::banner_say,
    &CVmBifTIO::banner_flush,
    &CVmBifTIO::banner_size_to_contents,
    &CVmBifTIO::banner_goto,
    &CVmBifTIO::banner_set_text_color,
    &CVmBifTIO::banner_set_screen_color,
    &CVmBifTIO::banner_get_info,
    &CVmBifTIO::banner_set_size,

    &CVmBifTIO::log_console_create,
    &CVmBifTIO::log_console_close,
    &CVmBifTIO::log_console_say
};

#endif /* VMBIF_DEFINE_VECTOR */
