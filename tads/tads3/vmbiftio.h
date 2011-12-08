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
    /* function vector */
    static vm_bif_desc bif_table[];

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
    static void log_input_event(VMG_ uint argc);

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
vm_bif_desc CVmBifTIO::bif_table[] =
{
    { &CVmBifTIO::say, 1, 0, TRUE },                                   /* 0 */
    { &CVmBifTIO::logging, 1, 1, FALSE },                              /* 1 */
    { &CVmBifTIO::clearscreen, 0, 0, FALSE },                          /* 2 */
    { &CVmBifTIO::more, 0, 0, FALSE },                                 /* 3 */
    { &CVmBifTIO::input, 0, 0, FALSE },                                /* 4 */
    { &CVmBifTIO::inputkey, 0, 0, FALSE },                             /* 5 */
    { &CVmBifTIO::inputevent, 0, 1, FALSE },                           /* 6 */
    { &CVmBifTIO::inputdialog, 5, 0, FALSE },                          /* 7 */
    { &CVmBifTIO::askfile, 4, 0, FALSE },                              /* 8 */
    { &CVmBifTIO::timedelay, 1, 0, FALSE },                            /* 9 */
    { &CVmBifTIO::sysinfo, 1, 0, TRUE },                              /* 10 */
    { &CVmBifTIO::status_mode, 1, 0, FALSE },                         /* 11 */
    { &CVmBifTIO::status_right, 1, 0, FALSE },                        /* 12 */
    { &CVmBifTIO::res_exists, 1, 0, FALSE },                          /* 13 */
    { &CVmBifTIO::set_script_file, 1, 1, FALSE },                     /* 14 */
    { &CVmBifTIO::get_charset, 1, 0, FALSE },                         /* 15 */
    { &CVmBifTIO::flush_output, 0, 0, FALSE },                        /* 16 */
    { &CVmBifTIO::input_timeout, 0, 1, FALSE },                       /* 17 */
    { &CVmBifTIO::input_cancel, 1, 0, FALSE },                        /* 18 */
    
    { &CVmBifTIO::banner_create, 8, 0, FALSE },                       /* 19 */
    { &CVmBifTIO::banner_delete, 1, 0, FALSE },                       /* 20 */
    { &CVmBifTIO::banner_clear, 1, 0, FALSE },                        /* 21 */
    { &CVmBifTIO::banner_say, 1, 0, TRUE },                           /* 22 */
    { &CVmBifTIO::banner_flush, 1, 0, FALSE },                        /* 23 */
    { &CVmBifTIO::banner_size_to_contents, 1, 0, FALSE },             /* 24 */
    { &CVmBifTIO::banner_goto, 3, 0, FALSE },                         /* 25 */
    { &CVmBifTIO::banner_set_text_color, 3, 0, FALSE },               /* 26 */
    { &CVmBifTIO::banner_set_screen_color, 2, 0, FALSE },             /* 27 */
    { &CVmBifTIO::banner_get_info, 1, 0, FALSE },                     /* 28 */
    { &CVmBifTIO::banner_set_size, 4, 0, FALSE },                     /* 29 */

    { &CVmBifTIO::log_console_create, 3, 0, FALSE },                  /* 30 */
    { &CVmBifTIO::log_console_close, 1, 0, FALSE },                   /* 31 */
    { &CVmBifTIO::log_console_say, 1, 0, TRUE },                      /* 32 */

    { &CVmBifTIO::log_input_event, 1, 0, FALSE }                      /* 33 */
};

#endif /* VMBIF_DEFINE_VECTOR */
