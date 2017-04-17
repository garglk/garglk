/* $Header$ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmconsol.h - TADS 3 console input reader and output formatter
Function
  Provides console input and output for the TADS 3 built-in function set
  for the T3 VM.

  T3 uses the UTF-8 character set to represent character strings.  The OS
  functions use the local character set.  We perform the mapping between
  UTF-8 and the local character set within this module, so that OS routines
  see local characters only, not UTF-8.

  This code is based on the TADS 2 output formatter, but has been
  substantially reworked for C++, Unicode, and the slightly different
  TADS 3 formatting model.
Notes
  
Modified
  09/04/99 MJRoberts  - Creation
*/

#ifndef VMCONSOL_H
#define VMCONSOL_H

#include <string.h>
#include <stdlib.h>
#include "wchar.h"

#include "os.h"
#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"


/* ------------------------------------------------------------------------ */
/*
 *   Synthetic console events.  These are events we generate alongside
 *   OS_EVT_xxx codes; we start these at 10000 to ensure that we don't
 *   overlap any current or future OS_EVT_xxx event codes.  
 */
#define VMCON_EVT_END_QUIET_SCRIPT   10000     /* end quiet script playback */
#define VMCON_EVT_DIALOG             10001            /* result from dialog */
#define VMCON_EVT_FILE               10002       /* result from file dialog */


/*
 *   Event attributes.  Some scriptable events have extra attributes
 *   associated with them in addition to the main payload.  These attributes
 *   take the syntactic form of HTML tag attributes, so in principal we could
 *   have arbitrarily many attributes, each of which have arbitrary string
 *   values.  However, at the moment, we only have a small number of
 *   attributes, and each attribute's value is merely a boolean, so it's
 *   adequate to represent these in the call interface with bit flags.  
 */

/* OVERWRITE, as in <file overwrite> */
#define VMCON_EVTATTR_OVERWRITE  0x00000001



/* ------------------------------------------------------------------------ */
/*
 *   Newline type codes.  These specify how we are to perform line
 *   breaking after writing out text.
 */
enum vm_nl_type
{
    /* 
     *   no line separation at all - write out this text and subsequent
     *   text as part of the same line with no separators 
     */
    VM_NL_NONE,

    /* 
     *   flushing in preparation for input - don't show any line separation,
     *   and make sure that we display everything in the buffer, including
     *   trailing spaces 
     */
    VM_NL_INPUT,

    /* break the line at the end of this text and start a newline */
    VM_NL_NEWLINE,

    /* OS line separation - add a space after the text */
    VM_NL_OSNEWLINE,

    /* 
     *   flushing internal buffers only: no line separation, and do not
     *   flush to underlying OS level yet 
     */
    VM_NL_NONE_INTERNAL
};


/* ------------------------------------------------------------------------ */
/*
 *   Handle manager.  This is a simple class for mapping system objects to
 *   integers, which we give as "handles" to byte code callers.  We give only
 *   the integer handles to byte code to ensure that handles given back to us
 *   by the byte code are valid; if we handed back raw pointers to the byte
 *   code, it could call us with random garbage, and we'd have no way to
 *   protect against it.  
 */
class CVmHandleManager
{
public:
    CVmHandleManager();

    /* 
     *   delete the object (use this rather than calling the destructor
     *   directly, since we need to call some virtuals in the course of
     *   preparing for deletion 
     */
    void delete_obj(VMG0_);

protected:
    /* delete */
    virtual ~CVmHandleManager();

    /* allocate a slot for a new item */
    int alloc_handle(void *item);

    /* clear the given handle's slot */
    void clear_handle(int handle)
    {
        /* if the handle is valid, clear its associated slot */
        if (is_valid_handle(handle))
            handles_[handle - 1] = 0;
    }

    /* is the given handle valid? */
    int is_valid_handle(int handle)
    {
        /* 
         *   it's valid if it's within range for the allocated slots, and
         *   there's a non-null object in the handle's slot 
         */
        return (handle >= 1
                && (size_t)handle <= handles_max_
                && handles_[handle - 1] != 0);
    }

    /* get the object for the given handle */
    void *get_object(int handle)
    {
        /* 
         *   If the handle is valid, get the item from the slot; the handle
         *   is indexed from 1, so decrement it to get the array index of the
         *   object for the handle.  If the handle is invalid, return null. 
         */
        return is_valid_handle(handle) ? handles_[handle - 1] : 0;
    }

    /* 
     *   Delete the item in a slot, in preparation for destroying the handle
     *   manager itself - each subclass must override this to do the
     *   appropriate work on termination.  
     */
    virtual void delete_handle_object(VMG_ int handle, void *obj) = 0;

    /* array of window banners */
    void **handles_;
    size_t handles_max_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Banner manager.  This keeps track of banner windows outside of the main
 *   console window.  
 */
class CVmBannerManager: public CVmHandleManager
{
public:
    CVmBannerManager() { }

    /*
     *   Create a banner window.  This creates an OS-level banner window, and
     *   creates a console object to format its output.  Returns a banner
     *   handle that can be used to refer to the window.  Banner handle zero
     *   is invalid and indicates failure.
     *   
     *   'parent_id' is the banner ID of the parent of the new banner.  The
     *   new banner is created as a child of the given parent.  If parent_id
     *   is zero, then the new banner is created as a child of the main
     *   window.  The parent determines how the new window is laid out: the
     *   new window's display area is carved out of the display area of the
     *   parent.
     *   
     *   'where' is OS_BANNER_FIRST, OS_BANNER_LAST, OS_BANNER_BEFORE, or
     *   OS_BANNER_AFTER.  'other_id' is the banner ID of an existing child
     *   of the given parent, for the relative insertion point; this is
     *   ignored for OS_BANNER_FIRST and OS_BANNER_LAST.
     *   
     *   'wintype' is an OS_BANNER_TYPE_xxx code giving the type of window to
     *   be created.
     *   
     *   'siz' is the size, in units specified by 'siz_units', an
     *   OS_BANNER_SIZE_xxx value.
     *   
     *   'style' is a combination of OS_BANNER_STYLE_xxx flags.  
     */
    int create_banner(VMG_ int parent_id,
                      int where, int other_id, int wintype,
                      int align, int siz, int siz_units,
                      unsigned long style);

    /* delete a banner */
    void delete_banner(VMG_ int banner)
        { delete_or_orphan_banner(vmg_ banner, FALSE); }

    /* 
     *   Get the OS-level handle for the banner - this handle can be used to
     *   call the os_banner_xxx functions directly.  
     */
    void *get_os_handle(int banner);

    /* get the banner's console object */
    class CVmConsoleBanner *get_console(int banner)
    {
        /* the object behind the handle is the console */
        return (CVmConsoleBanner *)get_object(banner);
    }

    /* flush all banners */
    void flush_all(VMG_ vm_nl_type nl);

protected:
    /* delete the object in a slot, in preparation for deleting the manager */
    virtual void delete_handle_object(VMG_ int handle, void *)
    {
        /* 
         *   delete the banner object, but orphan the system-level banner -
         *   this will allow the system-level banner to remain visible even
         *   after VM termination, in case the host application continues
         *   running even after the VM exits 
         */
        delete_or_orphan_banner(vmg_ handle, TRUE);
    }

    /* delete or orphan a banner window */
    void delete_or_orphan_banner(VMG_ int banner, int orphan);
};

/* ------------------------------------------------------------------------ */
/*
 *   Log console manager.  This keeps track of log consoles, which are
 *   consoles created specifically for capturing text to a log file.  
 */
class CVmLogConsoleManager: public CVmHandleManager
{
public:
    CVmLogConsoleManager() { }

    /* create a log console - returns the console handle */
    int create_log_console(VMG_ class CVmNetFile *nf, osfildef *fp,
                           class CCharmapToLocal *cmap, int width);

    /* delete a log console */
    void delete_log_console(VMG_ int handle);

    /* get the log's console object */
    class CVmConsoleLog *get_console(int banner)
    {
        /* the object behind the handle is the console */
        return (CVmConsoleLog *)get_object(banner);
    }

protected:
    /* delete the object associated with a handle */
    virtual void delete_handle_object(VMG_ int handle, void *)
    {
        /* delete the console */
        delete_log_console(vmg_ handle);
    }
};

/*
 *   Log console manager item.  We use these to track individual log
 *   consoles.  
 */
class CVmLogConsoleItem
{
public:
    CVmLogConsoleItem(const char *fname, class CCharmapToLocal *cmap);
    ~CVmLogConsoleItem();

    /* get the console */
    class CVmConsoleLog *get_console() const { return console_; }

protected:
    /* my console object */
    class CVmConsoleLog *console_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Script stack entry 
 */
struct script_stack_entry
{
    script_stack_entry(VMG_ script_stack_entry *encp, int old_more,
                       class CVmNetFile *netfile, osfildef *outfp,
                       const vm_val_t *filespec,
                       int new_more, int is_quiet, int is_event_script);

    void delobj(VMG0_);


    /* the enclosing stack level */
    script_stack_entry *enc;

    /* network file descriptor for our script file */
    class CVmNetFile *netfile;

    /* the script file at this level */
    osfildef *fp;

    /* the MORE mode that was in effect before this script file */
    int old_more_mode;

    /* the MORE mode in effect during this script */
    int more_mode;

    /* are we reading quietly from this script? */
    int quiet;

    /* is this an event script? */
    int event_script;

    /* VM global variable, for gc protection for our file spec */
    struct vm_globalvar_t *filespec;

private:
    ~script_stack_entry() { }
};


/* ------------------------------------------------------------------------ */
/*
 *   Console.  A console corresponds to device that shows information to
 *   and reads text from the user.  On a text system, the console is
 *   simply the terminal.  On a graphical system, the console is usually
 *   an application window.  
 */
class CVmConsole
{
public:
    CVmConsole();
    virtual void delete_obj(VMG0_);

    /* write out a null-terminated UTF-8 string */
    int format_text(VMG_ const char *p)
    {
        /* get its length and write it out */
        return format_text(vmg_ p, strlen(p));
    }

    /* write out a UTF-8 string of a given byte length */
    virtual int format_text(VMG_ const char *p, size_t len);

    /* format text explicitly to the log file, if any */
    int format_text_to_log(VMG_ const char *p, size_t len);

    /* display a blank line */
    void write_blank_line(VMG0_);

    /* set the whitespace mode (returns the old whitespace mode) */
    int set_obey_whitespace(int f);

    /* set the text color */
    void set_text_color(VMG_ os_color_t fg, os_color_t bg);

    /* set the body color */
    void set_body_color(VMG_ os_color_t color);

    /* set the caps flag - capitalize the next character */
    void caps();

    /* set the nocaps flag - make the next letter miniscule */
    void nocaps();

    /* flush the output with the given newline type */
    void flush(VMG_ vm_nl_type nl);

    /* flush the output, ending the current line and starting a new line */
    void flush(VMG0_) { flush(vmg_ VM_NL_NEWLINE); }

    /* empty our buffers */
    void empty_buffers(VMG0_);

    /* clear the window */
    virtual void clear_window(VMG0_) = 0;

    /* 
     *   Flush all windows we control.  By default, we just flush our own
     *   window; consoles that manage multiple windows should flush their
     *   managed windows here as well. 
     */
    virtual void flush_all(VMG_ vm_nl_type nl) { flush(vmg_ nl); }

    /* immediately update the display window */
    void update_display(VMG0_);

    /* 
     *   Open a new log file.  Closes any previous log file.  If the file
     *   already exists, we'll overwrite it with the new log information,
     *   otherwise we'll create a new file.  Returns zero on success,
     *   non-zero on failure.  
     */
    int open_log_file(VMG_ const char *fname);
    int open_log_file(VMG_ const vm_val_t *filespec,
                      const struct vm_rcdesc *rc);

    /* 
     *   close any existing log file - returns zero on success, non-zero
     *   on failure 
     */
    int close_log_file(VMG0_);

    /*
     *   Open a new command log file.  We'll log commands (and only
     *   commands) to the command log file.  Returns zero on success,
     *   non-zero on failure.  
     */
    int open_command_log(VMG_ const char *fname, int event_script);
    int open_command_log(VMG_ const vm_val_t *filespec,
                         const struct vm_rcdesc *rc, int event_script);

    /* close the command log file, if there is one */
    int close_command_log(VMG0_);

    /*
     *   Set the current MORE mode.  Returns the old state.  The state is
     *   true if we show MORE prompts, false if not.  The state will be
     *   false if the underlying OS display layer handles prompting, so a
     *   return of false doesn't necessarily mean that MORE prompts are
     *   never shown, but merely that we don't handle MORE prompts in the
     *   output formatter itself.  
     */
    virtual int set_more_state(int state) = 0;

    /* determine if we're in MORE mode */
    virtual int is_more_mode() const = 0;

    /* get the line width of the display device */
    virtual int get_line_width() const = 0;

    /* 
     *   Do we allow overrunning the line width when we can't find a natural
     *   breaking point (at a whitespace character, for example) such that
     *   we can fit some text within the line width?
     *   
     *   If this returns false, then we'll force a newline when we reach the
     *   line width, even if doing so breaks up a single word that doesn't
     *   have a natural breaking point within.  
     */
    virtual int allow_overrun() const = 0;

    /* get the page length of the display device */
    virtual int get_page_length() const = 0;

    /* get/set the double-space flag (for periods and other punctuation) */
    int get_doublespace() const { return doublespace_; }
    void set_doublespace(int f) { doublespace_ = f; }

    /* reset the MORE prompt line count */
    void reset_line_count(int clearing);

    /* 
     *   check to see if we're reading from a script input file - returns
     *   true if so, false if reading from the user (via the keyboard or
     *   other input device) 
     */
    int is_reading_script() const { return (script_sp_ != 0); }

    /* 
     *   check to see if we're reading quietly from a script - if we're
     *   reading from a script, and this flag is true, we suppress all
     *   output
     */
    int is_quiet_script() const
        { return (script_sp_ != 0 && script_sp_->quiet); }

    /* is the script in MORE mode? */
    int is_moremode_script() const
        { return (script_sp_ != 0 && script_sp_->more_mode); }

    /* is the script an <eventscript> type? */
    int is_event_script() const
        { return (script_sp_ != 0 && script_sp_->event_script); }

    /* 
     *   Open a script file.  If 'quiet' is true, no output is displayed
     *   while the script is being processed.  If 'script_more_mode' is true,
     *   MORE mode is in effect while processing the script, otherwise MORE
     *   mode is turned off while processing the script (to leave things as
     *   they are, simply pass in is_more_mode() for this argument).
     *   
     *   Returns 0 on success, non-zero on error.  
     */
    int open_script_file(VMG_ const char *fname,
                         int quiet, int script_more_mode);
    int open_script_file(VMG_ const vm_val_t *filespec,
                         const struct vm_rcdesc *rc,
                         int quiet, int script_more_mode);

    /* 
     *   Close the script file.  Returns the original MORE mode that was
     *   in effect before the script file was opened; this MORE mode
     *   should be restored.  
     */
    int close_script_file(VMG0_);

    /* 
     *   Read a line of text from the keyboard.  Fills in the buffer with
     *   a null-terminated UTF-8 string.  Returns zero on success,
     *   non-zero on end-of-file reading the console (which usually
     *   indicates that the user has closed the application, so we're in
     *   the process of terminating; it might also indicate that the
     *   user's terminal has been detached, in which case we also probably
     *   can't do much except terminate).  
     */
    int read_line(VMG_ char *buf, size_t buflen, int bypass_script = FALSE);

    /*
     *   Read a line of input with optional timeout.  Fills in the buffer
     *   with a null-terminated UTF-8 string.  Returns an OS_EVT_xxx code,
     *   according to how the input was terminated:
     *   
     *   OS_EVT_LINE - the user pressed Return to enter the text
     *.  OS_EVT_TIMEOUT - the timeout expired before the user pressed Return
     *.  OS_EVT_EOF - an error occurred reading the input
     *   
     *   This routine is a cover for the low-level os_gets_timeout(), and
     *   behaves essentially the same way.  Note in particular that if this
     *   routine returns OS_EVT_TIMEOUT, then our read_line_cancel() routine
     *   must be called before any output or other display changes can be
     *   made, with the exception that another call to read_line_timeout()
     *   is always allowed.  
     */
    int read_line_timeout(VMG_ char *buf, size_t buflen,
                          unsigned long timeout, int use_timeout,
                          int bypass_script = FALSE);

    /*
     *   Cancel a line of input in progress, which was interrupted by a
     *   timeout in read_line_timeout().  If 'reset' is true, we'll forget
     *   any editing state from the prior line.  
     */
    void read_line_cancel(VMG_ int reset);

    /*
     *   Display a file dialog.  This routine works exactly the same way
     *   as os_askfile(), but is implemented here to allow for a formatted
     *   text interface on systems where no dialog is available.  
     */
    int askfile(VMG_ const char *prompt, size_t prompt_len,
                char *reply, size_t replen,
                int dialog_type, os_filetype_t file_type,
                int bypass_script = FALSE);

    /*
     *   Display a system dialog.  This routine works exactly the same way
     *   as os_input_dialog(), but is implemented here to allow for a
     *   formatted text interface on systems where no dialog is available.
     */
    int input_dialog(VMG_ int icon_id, const char *prompt,
                     int standard_button_set,
                     const char **buttons, int button_count,
                     int default_index, int cancel_index,
                     int bypass_script = FALSE);

    /* show the MORE prompt and wait for the user to acknowledge it */
    virtual void show_more_prompt(VMG0_) = 0;

    /*
     *   Log an event.  This saves the event to the current script log, if
     *   there is one, in the proper format for the script.  We return the
     *   event code.
     *   
     *   'evt' is the event type, as an OS_EVT_xxx or VMCON_EVT_xxx code.
     *   
     *   'param' can be given in the local UI character set or in UTF-8 -
     *   specify which it is via 'param_is_utf8'.  We write the file in the
     *   local UI character set, so if the parameter is given in UTF-8, we
     *   have to translate it.  
     */
    int log_event(VMG_ int evt,
                  const char *param, size_t paramlen, int param_is_utf8);
    int log_event(VMG_ int evt)
        { return log_event(vmg_ evt, 0, 0, FALSE); }

    /* log an event with the given event type tag */
    void log_event(VMG_ const char *tag, 
                   const char *param, size_t paramlen,
                   int param_is_utf8);

    /* read an event from an event script */
    int read_event_script(VMG_ int *evt, char *buf, size_t buflen,
                          const int *filter, int filter_cnt,
                          unsigned long *attrs);

protected:
    /* the destructor is protected - use delete_obj() to delete */
    virtual ~CVmConsole() { }

    /* 
     *   Service routine - show MORE prompt on this console.  This can be
     *   called from show_more_prompt() when a MORE prompt is desired at all
     *   in the subclassed console.  
     */
    void show_con_more_prompt(VMG0_);
    
    /* read a line from the script file */
    int read_line_from_script(char *buf, size_t buflen, int *evt);

    /* read the type tag from the next script event */
    int read_script_event_type(int *evt, unsigned long *attrs);

    /* skip to the next line of the script */
    void skip_script_line(osfildef *fp);

    /* 
     *   read a script parameter - this reads the rest of the line into the
     *   given buffer, and skips to the start of the next line in the script;
     *   returns true on success, false if we reach EOF before reading
     *   anything 
     */
    int read_script_param(char *buf, size_t buflen, osfildef *fp);

    /* internal routine to terminate line reading */
    void read_line_done(VMG0_);

    /* write utf-8 text to a file, mapping to the given file character set */
    void write_to_file(osfildef *fp, const char *txt,
                       class CCharmapToLocal *map);


    /* open a comand log file - internal version */
    int open_command_log(VMG_ class CVmNetFile *netfile, int event_script);

    /* open a script file - internal version */
    int open_script_file(VMG_ class CVmNetFile *netfile,
                         const vm_val_t *filspec,
                         int quiet, int script_more_mode);

    /* our current display stream */
    class CVmFormatter *disp_str_;

    /* our log stream - this stream is written to the log file, if any */
    class CVmFormatterLog *log_str_;
    
    /* 
     *   Flag: the log stream is enabled.  We can temporarily disable the
     *   log stream, such as when writing to the statusline stream.  
     */
    unsigned int log_enabled_ : 1;
    
    /*
     *   Flag: display two spaces after a period-like punctuation mark.
     *   This should be true if the output should have two spaces after a
     *   period, question mark, or exclamation point, false for a single
     *   space.  This should generally be true for fixed-width fonts,
     *   false for proportional fonts, although some users might prefer to
     *   use single-spacing even for fixed-width fonts.  
     */
    unsigned int doublespace_ : 1;

    /* 
     *   flag: the command log is an event script; if this is set, we log all
     *   input events in the tagged <eventscript> file format, otherwise we
     *   log just command lines in the old-style ">line" format 
     */
    unsigned int command_eventscript_ : 1;

    /*
     *   Script-input stack.  Each time we open a script, we create a new
     *   stack entry object and link it at the head of the list.  So, the
     *   head of the list is the current state, the next element is the
     *   enclosing state, and so on.  
     */
    script_stack_entry *script_sp_;
    
    /* command log file, if there is one */
    class CVmDataSource *command_fp_;
    class CVmNetFile *command_nf_;
    struct vm_globalvar_t *command_glob_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Main system console.  This console is attached to the OS-level primary
 *   console.  
 */
class CVmConsoleMain: public CVmConsole
{
public:
    CVmConsoleMain(VMG0_);

    /* delete */
    void delete_obj(VMG0_);

    /* get the system banner manager */
    class CVmBannerManager *get_banner_manager() const
        { return banner_manager_; }

    /* get the system log console manager */
    class CVmLogConsoleManager *get_log_console_manager() const
        { return log_console_manager_; }
    
    /*
     *   Switch in or out of statusline mode.  When we're running on the text
     *   implementation of the OS layer, we must explicitly switch modes
     *   between the main text stream and statusline stream.  'mode' is true
     *   to switch to statusline mode, false to switch back to main text
     *   mode.  
     */
    void set_statusline_mode(VMG_ int mode);

    /* clear the window */
    virtual void clear_window(VMG0_);

    /* set MORE mode */
    virtual int set_more_state(int state)
    {
        int old_state;

        /* remember the old state */
        old_state = G_os_moremode;

        /* set the new mode */
        G_os_moremode = state;

        /* return the previous state */
        return old_state;
    }

    /* get the MORE mode */
    virtual int is_more_mode() const { return G_os_moremode; }

    /* 
     *   Flush everything - this flushes not only the main console, but any
     *   banner windows we're managing.  This should be called before pausing
     *   for input or for a timed delay, to make sure that buffered output in
     *   all windows is shown.  
     */
    void flush_all(VMG_ vm_nl_type nl);

    /* get the line width of the display device */
    virtual int get_line_width() const { return G_os_linewidth; }

    /* do not allow overrunning the line width on the main console */
    virtual int allow_overrun() const { return FALSE; }

    /* get the page length of the display device */
    virtual int get_page_length() const { return G_os_pagelength; }

    /* show the MORE prompt */
    virtual void show_more_prompt(VMG0_) { show_con_more_prompt(vmg0_); }

protected:
    /* main text area display stream */
    class CVmFormatterMain *main_disp_str_;

    /* statusline display stream */
    class CVmFormatterStatline *statline_str_;

    /* 
     *   The system banner window manager.  Since the main console is
     *   inherently a singleton (as there's only one OS-level primary
     *   console), we keep track of the banner manager.  
     */
    class CVmBannerManager *banner_manager_;

    /* the system log console manager */
    class CVmLogConsoleManager *log_console_manager_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Banner-window console.  
 */
class CVmConsoleBanner: public CVmConsole
{
public:
    /* create */
    CVmConsoleBanner(void *banner_handle, int win_type, unsigned long style);

    /* delete */
    void delete_obj(VMG0_);

    /* retrieve our OS-level banner handle */
    void *get_os_handle() const { return banner_; }

    /* get banner information */
    int get_banner_info(os_banner_info_t *info);

    /* clear the window */
    virtual void clear_window(VMG0_);

    /* set MORE mode */
    virtual int set_more_state(int state)
    {
        /* banners never change the global MORE mode state */
        return is_more_mode();
    }

    /* get the MORE mode - return the global mode flag */
    virtual int is_more_mode() const { return G_os_moremode; }

    /* show the MORE prompt - does nothing for a banner window */
    virtual void show_more_prompt(VMG0_) { show_con_more_prompt(vmg0_); }

    /* get the line width of the display device */
    virtual int get_line_width() const
        { return os_banner_get_charwidth(banner_); }

    /* allow overrunning the line width in a banner */
    virtual int allow_overrun() const { return TRUE; }

    /* get the page length of the display device, for MORE mode */
    virtual int get_page_length() const
        { return os_banner_get_charheight(banner_); }

protected:
    /* our underlying OS-level banner handle */
    void *banner_;

    /* our window type (an OS_BANNER_TYPE_xxx code) */
    int win_type_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Common base class for log-only consoles 
 */
class CVmConsoleLogBase: public CVmConsole
{
public:
    /* clear the window - do nothing on a log console */
    virtual void clear_window(VMG0_) { }

    /* set MORE mode - doesn't apply to us */
    virtual int set_more_state(int state) { return FALSE; }

    /* show the MORE prompt - does nothing */
    virtual void show_more_prompt(VMG0_) { }

    /* get the MORE mode */
    virtual int is_more_mode() const { return FALSE; }

    /* get the line width of the display device */
    virtual int get_line_width() const { return G_os_linewidth; }

    /* allow overrunning the line width in a lot file */
    virtual int allow_overrun() const { return TRUE; }

    /* 
     *   get the page length of the display device - this is arbitrary, since
     *   we don't use MORE mode anyway 
     */
    virtual int get_page_length() const { return 55; }
};

/*
 *   Log console.  This is used to create a console that has no display
 *   presence, but simply captures its output directly to a log file.  (This
 *   is similar to the log file attached to a regular display console, but
 *   this kind of console ONLY has the log file.)  
 */
class CVmConsoleLog: public CVmConsoleLogBase
{
public:
    /* create */
    CVmConsoleLog(VMG_ CVmNetFile *nf, osfildef *fp,
                  class CCharmapToLocal *cmap, int width);

    void delete_obj(VMG0_);

protected:
};

/*
 *   A special console that sends output to the *main* console's log file.
 *   This allows code to be written with a generic console pointer, rather
 *   than a test checking for the main console or some other console; just
 *   plug this in when there's not another console pointer and output will go
 *   to the main console automatically.  
 */
class CVmConsoleMainLog: public CVmConsoleLogBase
{
public:
    /* create */
    CVmConsoleMainLog() { }

    /* we send text to the main console's log */
    int format_text(VMG_ const char *p, size_t len)
    {
        /* send the text to the main console's log file */
        return G_console->format_text_to_log(vmg_ p, len);
    }

protected:
    ~CVmConsoleMainLog() { }
};


/* ------------------------------------------------------------------------ */
/*
 *   constants 
 */

/*
 *   HTML lexical analysis modes.  We use these modes for two purposes:
 *   
 *   First, for tracking our state while doing our own HTML parsing, which we
 *   do when the underlying renderer handles only plain text (in which case
 *   we must interpret and remove any HTML tags from the stream before
 *   sending the stream on to the underlying renderer).
 *   
 *   Second, for tracking the lexical state in the underlying renderer, when
 *   the underlying renderer handles full HTML interpretation.  In this case,
 *   we simply pass HTML tags through to the underlying renderer; but even
 *   though we don't interpret the tags, we do keep track of the lexical
 *   structure of the text, so that we can tell when we're inside a tag and
 *   when we're in ordinary text.  Certain operations we apply (such as case
 *   conversions with "\^" and "\v" sequences) apply only to ordinary text,
 *   so we need to know what's what in the stream text.  
 */
enum vmconsole_html_state
{
    VMCON_HPS_NORMAL,                          /* normal text, not in a tag */
    VMCON_HPS_TAG_START,                      /* parsing the start of a tag */
    VMCON_HPS_TAG_NAME,                               /* parsing a tag name */
    VMCON_HPS_TAG,                                  /* parsing inside a tag */
    VMCON_HPS_ATTR_NAME,                       /* parsing an attribute name */
    VMCON_HPS_ATTR_VAL,                       /* parsing an attribute value */
    VMCON_HPS_SQUOTE,                 /* in a single-quoted string in a tag */
    VMCON_HPS_DQUOTE,                 /* in a double-quoted string in a tag */
    VMCON_HPS_ENTITY_1ST,              /* first character in an entity name */
    VMCON_HPS_ENTITY_NUM_1ST,       /* first character of a numeric entitiy */
    VMCON_HPS_ENTITY_HEX,                /* in a hexadecimal entitiy number */
    VMCON_HPS_ENTITY_DEC,                     /* in a decimal entity number */
    VMCON_HPS_ENTITY_NAME,                             /* in a named entity */
    VMCON_HPS_MARKUP_END                   /* at last character of a markup */
};

/*
 *   HTML parsing mode flag for <BR> tags.  We defer these until we've
 *   read the full tag in order to obey an HEIGHT attribute we find.  When
 *   we encounter a <BR>, we figure out whether we think we'll need a
 *   flush or a blank line; if we find a HEIGHT attribute, we may change
 *   this opinion.  
 */
enum vmconsole_html_br_mode
{
    HTML_DEFER_BR_NONE,                                  /* no pending <BR> */
    HTML_DEFER_BR_FLUSH,                       /* only need to flush output */
    HTML_DEFER_BR_BLANK                                /* need a blank line */
};

/*
 *   Color/attribute information.  Each character is tagged with its current
 *   color and attributes. 
 */
struct vmcon_color_t
{
    /* foreground color */
    os_color_t fg;

    /* background color */
    os_color_t bg;

    /* the current OS_ATTR_xxx attributes */
    int attr;

    /* check for equality with another color structure */
    int equals(const vmcon_color_t *other) const
    {
        return (fg == other->fg
                && bg == other->bg
                && attr == other->attr);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Output Buffer Flags.  Each character in the output buffer has an
 *   associated flag value associated with it; the flag value is a
 *   combination of the bit flags defined here. 
 */

/*
 *   Unbreakable character point.  This indicates that a line break is not
 *   allowed to follow this character, even if the text at this point would
 *   ordinarily allow a soft line break.  Note that this does NOT override a
 *   hard line break (i.e., an explicit newline); it merely prevents a soft
 *   line break from occurring immediately after this character.
 *   
 *   We apply this flag to the character immediately preceding an explicit
 *   non-breaking space in the text stream.  
 */
#define VMCON_OBF_NOBREAK    0x01

/*
 *   Breakable character point.  This indicates that a line break is allowed
 *   to follow this character, even if the text at this point would not
 *   ordinarily allow a soft line break.
 *   
 *   We apply this flag to the character immediately preceding an explicit
 *   zero-width breakable space in the text stream. 
 */
#define VMCON_OBF_OKBREAK    0x02

/*
 *   Break-anywhere mode.  This indicates that this character is part of a
 *   run of break-anywhere text.  In break-anywhere text, we're allowed to
 *   break lines between any two characters, except adjacent to explicit
 *   non-breaking spaces in the text stream.
 *   
 *   Note that the NOBREAK flag overrides this flag, because this flag
 *   merely indicates the mode, while the NOBREAK flag indicates an explicit
 *   non-breaking indicator.  
 */
#define VMCON_OBF_BREAK_ANY  0x04

/*
 *   Soft hyphenation point.  This flag indicates that the text can be
 *   hyphenated immediately following this character, which is to say that
 *   we can insert a hyphen following this character and break the line
 *   after the hyphen.  If we do not hyphenate here, the soft hyphen has no
 *   effect; in particular, no hyphen character appears in the output stream
 *   when a soft hyphen is not used as a line break point.  
 */
#define VMCON_OBF_SHY        0x08

/*
 *   Quoted space.  This indicates that this is a space character that
 *   doesn't collapse in runs of contiguous whitespace. 
 */
#define VMCON_OBF_QSPACE     0x10


/* ------------------------------------------------------------------------ */
/*
 *   Output formatter stream interface.  A formatter stream performs
 *   formatting on a stream of displayed text.
 *   
 *   A given stream of display text might be fed into more than one
 *   formatter stream.  For example, if logging is turned on for a
 *   console, we'll feed the same text to the console's main formatting
 *   stream, which will end up being displayed to the user, and also to
 *   the log file's formatting stream, which will end up written out to
 *   the log file.
 *   
 *   Note that the formatter interface is internal to the console system.
 *   Client code should never need to refer to a formatter object, but
 *   should instead call the console object (CVmConsole).  
 */

/* <TAB> tag alignment types */
enum vmfmt_tab_align_t
{
    VMFMT_TAB_NONE,
    VMFMT_TAB_LEFT,
    VMFMT_TAB_CENTER,
    VMFMT_TAB_RIGHT,
    VMFMT_TAB_DECIMAL
};

/* maximum formatter state stack depth */
const size_t CVMFMT_STACK_MAX = 25;

/* maximum depth of color stack */
const size_t CVFMT_CLRSTK_MAX = 25;

/* maximum length of an attribute name/value */
const size_t CVFMT_MAX_ATTR_NAME = 40;
const size_t CVFMT_MAX_ATTR_VAL = 256;

/* output stream information structure (forward declaration) */
typedef struct out_stream_info out_stream_info;

/* output formatter stream */
class CVmFormatter
{
public:
    CVmFormatter(class CVmConsole *console)
    {
        /* remember our display object */
        console_ = console;

        /* there's no title buffer by default */
        html_title_buf_ = 0;
        html_title_buf_size_ = 0;

        /* we have no horizontal tab table yet (for the HTML mini-parser) */
        tabs_ = 0;

        /* no character mapper yet */
        cmap_ = 0;
    }

    /* 
     *   delete the object (call this instead of calling the destructor
     *   directly, since some subclasses need global access)
     */
    virtual void delete_obj(VMG0_) { delete this; }

    /* initialize */
    virtual void init()
    {
        /* start out at the first column */
        linepos_ = 0;
        linecol_ = 0;
        linebuf_[0] = '\0';
        is_continuation_ = FALSE;

        /* start out in normal text color */
        cur_color_.fg = OS_COLOR_P_TEXT;
        cur_color_.bg = OS_COLOR_P_TRANSPARENT;
        cur_color_.attr = 0;
        os_color_ = cur_color_;

        /* no pending tab yet */
        pending_tab_align_ = VMFMT_TAB_NONE;

        /* start out at the first line */
        linecnt_ = 0;

        /* we're not in either "caps", "nocaps", or "allcaps" mode yet */
        capsflag_ = nocapsflag_ = allcapsflag_ = FALSE;

        /* 
         *   set the initial buffer flags: start out in word-break (not
         *   break-anywhere) more 
         */
        cur_flags_ = 0;

        /* 
         *   start in normal spacing mode - treat runs of whitespace as
         *   equivalent to a single space 
         */
        obey_whitespace_ = FALSE;

        /* start out in HTML interpretation mode */
        literal_mode_ = FALSE;

        /* presume we won't have OS-level line wrapping */
        os_line_wrap_ = FALSE;

        /* we haven't flushed a new line yet */
        just_did_nl_ = FALSE;

        /* assume that the underlying system is not HTML-enabled */
        html_target_ = FALSE;

        /* presume this target accepts OS highlighting sequences */
        plain_text_target_ = FALSE;

        /* 
         *   start out in "normal" lexical state in our parser and in the
         *   underlying output stream 
         */
        html_parse_state_ = VMCON_HPS_NORMAL;
        html_passthru_state_ = VMCON_HPS_NORMAL;

        /* not in an ignored tag yet (-> nesting depth is zero) */
        html_in_ignore_ = 0;

        /* not in title mode yet (-> nesting depth is zero) */
        html_in_title_ = 0;

        /* not yet deferring line breaks */
        html_defer_br_ = HTML_DEFER_BR_NONE;

        /* not yet in quotes (-> nesting depth is zero) */
        html_quote_level_ = 0;

        /* not in a PRE section */
        html_pre_level_ = 0;

        /* we're not parsing inside any tags yet */
        html_allow_alt_ = FALSE;
        html_allow_color_ = FALSE;
        html_in_body_ = FALSE;
        html_in_tab_ = FALSE;
        html_in_wrap_ = FALSE;

        /* assume we're a normal display stream */
        is_disp_stream_ = TRUE;

        /* no stacked colors yet */
        color_sp_ = 0;
    }

    /* get/set obey-whitespace mode */
    int get_obey_whitespace() const { return obey_whitespace_ != 0; }
    void set_obey_whitespace(int f) { obey_whitespace_ = (f != 0); }

    /* turn on CAPS mode */
    void caps()
    {
        /* turn on CAPS mode */
        capsflag_ = TRUE;

        /* turn off the NOCAPS and ALLCAPS modes */
        nocapsflag_ = FALSE;
        allcapsflag_ = FALSE;
    }

    /* turn on NOCAPS mode */
    void nocaps()
    {
        /* turn on NOCAPS mode */
        nocapsflag_ = TRUE;
        
        /* turn off CAPS and ALLCAPS mode */
        capsflag_ = FALSE;
        allcapsflag_ = FALSE;
    }
    
    /* turn on or off ALLCAPS mode */
    void allcaps(int all_caps)
    {
        /* set the ALLCAPS flag */
        allcapsflag_ = all_caps;
        
        /* clear the CAPS and NOCAPS flags */
        capsflag_ = FALSE;
        nocapsflag_ = FALSE;
    }

    /* 
     *   Display a string of a given byte-length.  The text is given in
     *   UTF-8 format.  We'll interpret embedded control codes (newlines,
     *   blank lines, caps flags, etc); depending on the display mode, we
     *   might also interpret HTML markup sequences.  
     */
    int format_text(VMG_ const char *s, size_t len);

    /* set the text attributes for subsequent format_text() displays */
    void set_text_attr(VMG_ int attr)
    {
        /* remember the new text attributes */
        cur_color_.attr = attr;
    }

    /* set the text color for subsequent format_text() displays */
    void set_text_color(VMG_ os_color_t fg, os_color_t bg)
    {
        /* remember the new text colors */
        cur_color_.fg = fg;
        cur_color_.bg = bg;
    }

    /* write a blank line to the stream */
    void write_blank_line(VMG0_);

    /*
     *   Flush the current line to the display, using the given type of
     *   line termination. 
     */
    void flush(VMG_ vm_nl_type nl);

    /* clear our buffers */
    void empty_buffers(VMG0_);

    /* immediately update the display window */
    void update_display(VMG0_);

    /* 
     *   determine if I'm an HTML target - this returns true if the
     *   underyling renderer accepts HTML, in which case we write HTML
     *   markups directly to the os_xxx routines (os_printz, for example).
     */
    int is_html_target() const { return html_target_; }

    /* reset the MORE prompt line counter */
    virtual void reset_line_count(int clearing) { linecnt_ = 0; }

    /*
     *   Note that we read a line from the keyboard with echo.  Reading a
     *   line displays a carriage return without our help, taking us back to
     *   the first column - we need to know this so that we can deal
     *   properly with output on the following line.  
     */
    void note_input_line()
    {
        /* 
         *   a CR/LF will have been echoed automatically by the input
         *   reader, which takes us back to the first column 
         */
        linecol_ = 0;

        /* note that we effectively just wrote a newline */
        just_did_nl_ = TRUE;
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Virtual interface to underlying OS renderer 
     */

    /* turn HTML mode on/off in the underlying OS-level renderer */
    virtual void start_html_in_os() = 0;
    virtual void end_html_in_os() = 0;

    /* set the text color */
    virtual void set_os_text_color(os_color_t fg, os_color_t bg) = 0;

    /* set the body color */
    virtual void set_os_body_color(os_color_t) = 0;

    /* set text attributes */
    virtual void set_os_text_attr(int attr) = 0;

    /* 
     *   display text to the OS window; the text is given in the local
     *   character set 
     */
    virtual void print_to_os(const char *txt) = 0;

    /* flush the underlying OS-level rendere */
    virtual void flush_to_os() = 0;

    /* set the window title in the OS layer */
    virtual void set_title_in_os(const char *txt) = 0;

    /* set a new character mapper */
    void set_charmap(class CCharmapToLocal *cmap);

protected:
    virtual ~CVmFormatter();

    /*
     *   Write a line (or a partial line) of text to the stream, using the
     *   indicated line breaking.  The text is given as wide Unicode
     *   characters.  
     */
    void write_text(VMG_ const wchar_t *txt, size_t cnt,
                    const vmcon_color_t *colors, vm_nl_type nl);

    /* write a tab to the stream */
    void write_tab(VMG_ int indent, int multiple);

    /* flush the current line, with or without padding */
    void flush_line(VMG_ int padding);

    /* 
     *   Buffer a character of output.  The character is presented to us as a
     *   wide Unicode character.  We'll expand this character with the local
     *   character mapping's expansion rules, then add the expansion to our
     *   output buffer, performing word-wrapping as needed.  
     */
    void buffer_char(VMG_ wchar_t c);

    /* 
     *   buffer an expanded character - we'll buffer the given unicode
     *   character, with no further expansion 
     */
    void buffer_expchar(VMG_ wchar_t c);

    /* buffer a rendered expanded character */
    void buffer_rendered(wchar_t c, unsigned char flags, int wid);

    /* 
     *   Buffer a string of output to the stream.  The string is in UTF-8
     *   format. 
     */
    void buffer_string(VMG_ const char *txt);

    /* buffer a wide unicode character string */
    void buffer_wstring(VMG_ const wchar_t *txt);

    /* get the next wide unicode character in a UTF8-encoded string */
    static wchar_t next_wchar(const char **s, size_t *len);

    /* 
     *   Determine if we should use MORE mode in the formatter layer; if
     *   this returns true, we handle MORE prompting ourselves, otherwise
     *   we handle it through the OS layer.
     *   
     *   The default version of this function is implemented in the output
     *   formatter configuration module.  This function can also be
     *   overridden in a subclass (for example, a log stream never uses
     *   MORE mode, no matter what configuration we're using).  
     */
    virtual int formatter_more_mode() const;

    /*
     *   Get the maximum column to store in our internal buffer.  If we're
     *   doing our own line breaking, we'll break off the buffer at the
     *   actual line width of the underlying console.  If we're doing OS
     *   line wrapping, we'll simply fill up our internal buffer to its
     *   maximum size, since the flushing points are irrelevant to the line
     *   wrapping the underlying OS console will be doing and hence we might
     *   as well buffer as much as we can for efficiency.  
     */
    virtual int get_buffer_maxcol() const
    {
        /* 
         *   if the OS is doing the line wrapping, use the full buffer;
         *   otherwise, use the actual line width from the underyling
         *   console 
         */
        if (os_line_wrap_)
            return OS_MAXWIDTH;
        else
            return console_->get_line_width();
    }

    /*
     *   Determine if the underlying stream interprets HTML markups.  We
     *   call this during initialization to set our html_target_ member
     *   (we cache the result since we check it frequently).
     *   
     *   This is implemented in the formatter configuration module.  
     */
    virtual int get_init_html_target() const;

    /* -------------------------------------------------------------------- */
    /*
     *   HTML mini-parser.  This only needs to be implemented when linking
     *   with non-HTML osifc implementations; when osifc provides HTML
     *   parsing, we don't need to do any HTML parsing here, so these can use
     *   dummy implementations. 
     */

    /*
     *   Resume text-only HTML mini-parser.  This is called when we start
     *   writing a new string and discover that we're parsing inside an HTML
     *   tag in our mini-parser.
     *   
     *   Returns the next character after the run of text we parse.  
     */
    wchar_t resume_html_parsing(VMG_ wchar_t c,
                                const char **sp, size_t *slenp);

    /* parse an HTML entity ('&') markup */
    wchar_t parse_entity(VMG_ wchar_t *ent, const char **sp, size_t *slenp);
    
    /*
     *   Parse the beginning HTML markup.  This is called when we are
     *   scanning a '<' or '&' character in output text, and we're in HTML
     *   mode, and the underlying target doesn't support HTML parsing.
     *   Returns the next character to process after we finish our initial
     *   parsing.  
     */
    wchar_t parse_html_markup(VMG_ wchar_t c,
                              const char **sp, size_t *slenp);

    /* 
     *   Expand any pending tab.  This should be called when we're doing
     *   HTML mini-parsing, and we see a <TAB> tag or we reach the end of an
     *   output line.  We'll expand any pending RIGHT/CENTER tab by going
     *   back to the tab's location and inserting the necessary number of
     *   spaces now that we know the extent of the text affected.
     *   
     *   If 'allow_anon' is true, we will allow "anonymous" tabs, which is
     *   to say tabs with no ID attribute.  Anonymous tabs allow text to be
     *   tabbed relative to the full width of the line, but these are
     *   meaningful only with normal line endings; when a line is flushed
     *   before the end of the line is reached (because the line will be
     *   used for a prompt, for example), anonymous tabs should not be
     *   expanded with the line.  
     */
    void expand_pending_tab(VMG_ int allow_anon);

    /* 
     *   find a tab definition object; if 'create' it true and the specified
     *   tab doesn't exist, we'll create a new tab object
     */
    class CVmFmtTabStop *find_tab(wchar_t *id, int create);

    /* service routine - translate a wide character string to an integer */
    static int wtoi(const wchar_t *p);

    /* parse a COLOR attribute of a FONT tag */
    int parse_color_attr(VMG_ const wchar_t *val, os_color_t *result);

    /* push the current color for later restoration */
    void push_color()
    {
        /* 
         *   add a level to the color stack, if possible; if it's not
         *   possible, assume we've lost an end tag somewhere, so rotate the
         *   entire stack down a level 
         */
        if (color_sp_ == CVFMT_CLRSTK_MAX)
        {
            /* take everything down a level */
            memmove(color_stack_, color_stack_ + 1,
                    (CVFMT_CLRSTK_MAX - 1)*sizeof(color_stack_[0]));
            --color_sp_;
        }

        /* save the current color in the stack */
        color_stack_[color_sp_++] = cur_color_;
    }

    /* pop the current color */
    void pop_color()
    {
        /* 
         *   if we're at the bottom of the stack, we must have had too many
         *   close tags; ignore the extra operation 
         */
        if (color_sp_ != 0)
        {
            /* restore the next color down */
            cur_color_ = color_stack_[--color_sp_];
        }
    }

    /* 
     *   Process a <nolog> tag.  By default, we ignore this tag entirely.
     *   Log streams should hide the contents of this tag.  
     */
    virtual void process_nolog_tag(int /*is_end_tag*/)
    {
        /* by default, ignore this tag */
    }

    /*
     *   Process a <log> tag.  By default, we hide the contents of this tag;
     *   this tag marks text that is to be shown only in a log stream. 
     */
    virtual void process_log_tag(int is_end_tag)
    {
        /* turn hiding on or off as appropriate */
        if (is_end_tag)
            --html_in_ignore_;
        else
            ++html_in_ignore_;
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Member variables 
     */
        
    /* the console that owns this formatter stream */
    class CVmConsole *console_;

    /* our character mapper */
    class CCharmapToLocal *cmap_;
    
    /* current line position and output column */
    int linepos_;
    int linecol_;

    /* 
     *   flag: the current buffer is a "continuation" line; that is, we've
     *   already flushed a partial line to the display without moving to a
     *   new line vertically, and the current buffer will be displayed on the
     *   same line on the terminal, to the right of the previously-displayed
     *   material 
     */
    int is_continuation_;

    /* number of lines on the screen (since last MORE prompt) */
    int linecnt_;

    /* 
     *   Output buffer.  We keep the output buffer as wide Unicode
     *   characters, and translate to the local character set when we
     *   flush the buffer and send the text to the os_xxx routines for
     *   display. 
     */
    wchar_t linebuf_[OS_MAXWIDTH + 1];

    /* 
     *   Output buffer character colors.  We keep track of the display color
     *   of each character in the buffer.  
     */
    vmcon_color_t colorbuf_[OS_MAXWIDTH + 1];

    /* 
     *   output buffer flags - we keep a flag value for each character in
     *   the output buffer 
     */
    unsigned char flagbuf_[OS_MAXWIDTH + 1];

    /* current attribute name/value buffers */
    wchar_t attrname_[CVFMT_MAX_ATTR_NAME];
    wchar_t attrval_[CVFMT_MAX_ATTR_VAL];
    wchar_t attr_qu_;

    /* current color of characters being added to our buffer */
    vmcon_color_t cur_color_;

    /* color of last OS output (via write_text) */
    vmcon_color_t os_color_;

    /* stack of color attributes, saved for nested tags */
    vmcon_color_t color_stack_[CVFMT_CLRSTK_MAX];
    size_t color_sp_;

    /* 
     *   Current output character flags.  This is the base value to write to
     *   flagbuf_ for each character we output, given the current mode
     *   settings.  
     */
    unsigned char cur_flags_;

    /* obey-whitespace mode - treat whitespace as literal */
    unsigned int obey_whitespace_ : 1;

    /* literal mode - ignore HTML markups, pass everything literally */
    unsigned int literal_mode_ : 1;

    /* CAPS mode - next character output is converted to upper-case */
    unsigned int capsflag_ : 1;

    /* NOCAPS mode - next character output is converted to lower-case */
    unsigned int nocapsflag_ : 1;

    /* ALLCAPS mode - all characters output are converted to upper-case */
    unsigned int allcapsflag_ : 1;

    /* flag indicating that we just flushed a new line */
    unsigned int just_did_nl_ : 1;

    /*
     *   Flag indicating that the underlying output system wants to
     *   receive its output as HTML.
     *   
     *   If this is true, we'll pass through HTML to the underlying output
     *   system, and in addition generate HTML sequences for certain
     *   TADS-native escapes (for example, we'll convert the "\n" sequence
     *   to a <BR> sequence).
     *   
     *   If this is false, we'll do just the opposite: we'll remove HTML
     *   from the output stream and convert it into normal text sequences. 
     */
    unsigned int html_target_ : 1;

    /*
     *   Flag indicating that the target uses plain text.  If this flag is
     *   set, we won't add the OS escape codes for highlighted characters. 
     */
    unsigned int plain_text_target_ : 1;

    /* 
     *   flag: the underlying OS layer handles line wrapping, so we never
     *   need to write newlines when flushing our line buffer except when
     *   we want to indicate a hard line break 
     */
    unsigned int os_line_wrap_ : 1;

    /* 
     *   Current lexical analysis state for our own HTML parsing.  This is
     *   used to track our HTML state when we have an underlying plain text
     *   renderer.  
     */
    vmconsole_html_state html_parse_state_;

    /*
     *   Current lexical analysis mode for the text stream going to the
     *   underlying renderer.  This is used to track the lexical structure of
     *   the stream when we're passing HTML tags through to the underlying
     *   renderer.  
     */
    vmconsole_html_state html_passthru_state_;

    /* last tag name */
    char html_passthru_tag_[32];
    char *html_passthru_tagp_;

    /* <BR> defer mode */
    vmconsole_html_br_mode html_defer_br_;

    /* 
     *   HTML "ignore" mode - we suppress all output when parsing the
     *   contents of a <TITLE> or <ABOUTBOX> tag.  This is a counter that
     *   keeps track of the nesting depth for ignored tags.  
     */
    int html_in_ignore_;

    /*
     *   HTML <TITLE> mode - when we're in this mode, we're gathering the
     *   title (i.e., we're inside a <TITLE> tag's contents).  We'll copy
     *   characters to the title buffer rather than the normal output
     *   buffer, and then call os_set_title() when we reach the </TITLE>
     *   tag.  This is a counter that keeps track of the nesting depth of
     *   <TITLE> tags.  
     */
    int html_in_title_;

    /* buffer for the title */
    char *html_title_buf_;
    size_t html_title_buf_size_;

    /* pointer to next available character in title buffer */
    char *html_title_ptr_;

    /* 
     *   quoting level - this is a counter that keeps track of the nesting
     *   depth of <Q> tags 
     */
    int html_quote_level_;

    /* PRE nesting depth */
    int html_pre_level_;

    /*
     *   Parsing mode flag for ALT attributes.  If we're parsing a tag
     *   that allows ALT, such as IMG or SOUND, we'll set this flag, then
     *   insert the ALT text if we encounter it during parsing.  
     */
    unsigned int html_allow_alt_ : 1;

    /* parsing mode flag for COLOR attributes */
    unsigned int html_allow_color_ : 1;

    /* parsing a BODY tag's attributes */
    unsigned int html_in_body_ : 1;

    /* parsing a TAB tag's attributes */
    unsigned int html_in_tab_ : 1;

    /* parsing a WRAP tag's attributes */
    unsigned int html_in_wrap_ : 1;

    /* hash table of <TAB> objects */
    class CVmHashTable *tabs_;

    /* characteristics of TAB tag we're defining */
    class CVmFmtTabStop *new_tab_entry_;
    vmfmt_tab_align_t new_tab_align_;
    wchar_t new_tab_dp_;

    /* 
     *   Characteristics of pending tab.  We must handle this tab when we
     *   reach the next <TAB> or the end of the current line.  If
     *   pending_tab_align_ is VMFMT_TAB_NONE, it indicates that there is no
     *   pending tab (since a pending tab always requires alignment).  
     */
    vmfmt_tab_align_t pending_tab_align_;
    class CVmFmtTabStop *pending_tab_entry_;
    wchar_t pending_tab_dp_;

    /* 
     *   starting column of pending tab - this is the output column where we
     *   were writing when the pending tab was encountered, so this is the
     *   column where we insert spaces for the tab 
     */
    int pending_tab_start_;

    /* 
     *   color/attributes active at start of pending tab - this is the color
     *   we'll use for the spaces we insert when we insert the tab 
     */
    vmcon_color_t pending_tab_color_;

    /* 
     *   Flag: this is a display stream.  Other types of streams (such as
     *   log file streams) should set this to false. 
     */
    unsigned int is_disp_stream_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Formatter for display windows 
 */
class CVmFormatterDisp: public CVmFormatter
{
public:
    CVmFormatterDisp(class CVmConsole *console)
        : CVmFormatter(console)
    {
    }

    /* initialize */
    void init()
    {
        /* inherit base class initialization */
        CVmFormatter::init();

        /* 
         *   if we're compiled for HTML mode, set the standard output
         *   stream so that it knows it has an HTML target - this will
         *   ensure that HTML tags are passed through to the underlying
         *   stream, and that we generate HTML equivalents for our own
         *   control sequences 
         */ 
        html_target_ = get_init_html_target();

        /* 
         *   since we always use HTML mode, turn on HTML mode in the
         *   underlying OS window if our underlying OS renderer is HTML-aware
         */
        if (html_target_)
            start_html_in_os();
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Formatter subclass for the main display 
 */
class CVmFormatterMain: public CVmFormatterDisp
{
public:
    CVmFormatterMain(class CVmConsole *console, size_t html_title_buf_size)
        : CVmFormatterDisp(console)
    {
        /* allocate a title buffer */
        html_title_buf_size_ = html_title_buf_size;
        if (html_title_buf_size_ != 0)
            html_title_buf_ = (char *)t3malloc(html_title_buf_size_);
    }
    
    /* initialize */
    void init()
    {
        /* inherit base class initialization */
        CVmFormatterDisp::init();

        /* remember the OS line wrap setting from the console */
        os_line_wrap_ = get_os_line_wrap();
    }

    /* set the window title in the OS layer */
    virtual void set_title_in_os(const char *txt)
    {
        /* set the window title */
        os_set_title(txt);
    }

protected:
    ~CVmFormatterMain()
    {
        /* delete our title buffer */
        if (html_title_buf_ != 0)
            t3free(html_title_buf_);
    }

    /* 
     *   Determine if the main console uses OS-level line wrapping - if this
     *   is returns true, then an output formatter on this console will not
     *   insert a newline at the end of a line that it's flushing for word
     *   wrapping, but will instead let the underlying OS display layer
     *   handle the wrapping.
     *   
     *   The OS line wrapping status is a PERMANENT feature of the console,
     *   so it is safe for the formatter to query this during initialization
     *   and cache the value.  
     */
    static int get_os_line_wrap();

    /* turn HTML mode on/off in the underlying OS-level renderer */
    virtual void start_html_in_os();
    virtual void end_html_in_os();

    /* display text directly to the OS renderer */
    virtual void print_to_os(const char *txt)
    {
        /* display the text on the primary OS console */
        os_printz(txt);
    }

    /* flush the underlying OS-level renderer */
    virtual void flush_to_os() { os_flush(); }

    /* 
     *   set text attributes for subsequent text directly in the underlying
     *   OS window 
     */
    virtual void set_os_text_attr(int attr)
    {
        /* if the target isn't in 'plain' mode, set the attributes */
        if (!plain_text_target_)
            os_set_text_attr(attr);
    }

    /* set the text color in the underlying OS window */
    virtual void set_os_text_color(os_color_t fg, os_color_t bg)
    {
        /* 
         *   if the target is in 'plain' mode, don't use colors; otherwise,
         *   ask the console to do the work 
         */
        if (!plain_text_target_)
            os_set_text_color(fg, bg);
    }

    /* set the "body" color in the underlying OS window */
    virtual void set_os_body_color(os_color_t color)
    {
        /* if not in "plain" mode, set the color */
        if (!plain_text_target_)
            os_set_screen_color(color);
    }

};

/* ------------------------------------------------------------------------ */
/*
 *   Formatter subclass for the status line 
 */
class CVmFormatterStatline: public CVmFormatterDisp
{
public:
    CVmFormatterStatline(class CVmConsole *console)
        : CVmFormatterDisp(console)
    {
    }

    /* we never use 'more' mode in the status line */
    virtual int formatter_more_mode() const { return FALSE; }

    /* HTML mode has no effect on the status line */
    virtual void start_html_in_os() { }
    virtual void end_html_in_os() { }

    /* text colors and attributes are not used in the status line */
    virtual void set_os_text_color(os_color_t, os_color_t) { }
    virtual void set_os_body_color(os_color_t) { }
    virtual void set_os_text_attr(int) { }

    /* text displayed in the status line goes directly to the main console */
    virtual void print_to_os(const char *txt) { os_printz(txt); }

    /* flushing the status line simply flushes the main text stream */
    virtual void flush_to_os() { os_flush(); }

    /* titles have no effect in the status line */
    virtual void set_title_in_os(const char *) { }
};

/* ------------------------------------------------------------------------ */
/*
 *   Formatter subclass for banner windows
 */
class CVmFormatterBanner: public CVmFormatterDisp
{
public:
    CVmFormatterBanner(void *banner, class CVmConsole *console,
                       int win_type, unsigned long style)
        : CVmFormatterDisp(console)
    {
        /* remember my OS banner handle */
        banner_ = banner;

        /* remember my window type */
        win_type_ = win_type;

        /* remember the banner style */
        style_ = style;
    }

    /* initialize */
    void init_banner(int os_line_wrap, int obey_whitespace, int literal_mode)
    {
        /* inherit base class initialization */
        CVmFormatterDisp::init();

        /* remember the OS line wrapping mode in our underlying OS window */
        os_line_wrap_ = os_line_wrap;

        /* remember the whitespace setting */
        obey_whitespace_ = obey_whitespace;

        /* remember the literal-mode setting */
        literal_mode_ = literal_mode;
    }

    /* set the window title in the OS layer (does nothing for a banner) */
    virtual void set_title_in_os(const char *) { }

    /* reset the MORE prompt line counter */
    virtual void reset_line_count(int clearing)
    {
        /* 
         *   To ensure we always keep a line of context when we page-forward
         *   from a MORE prompt, start the line counter at 2.  Note that we
         *   do this in banner windows, but not in the main window, because -
         *   for historical reasons - the OS layer tells us the *paging* size
         *   for the main window, but the *actual* height for banner window.
         *   Note that we don't want to handle this adjustment in our own
         *   page length calculation, because doing so would cause the
         *   *first* MORE prompt (from a cleared window) to show up too
         *   early.
         *   
         *   When we're clearing the screen, reset to zero, since we have no
         *   context to retain.  
         */
        linecnt_ = (clearing ? 0 : 1);
    }

protected:
    /* 
     *   Use MORE mode in a banner window if we have the MORE-mode banner
     *   window style, AND the base display banner would use MORE mode.  The
     *   latter check tests to see if we handle MORE mode at the OS level or
     *   in the formatter.  
     */
    virtual int formatter_more_mode() const
    {
        return ((style_ & OS_BANNER_STYLE_MOREMODE) != 0
                && CVmFormatterDisp::formatter_more_mode());
    }

    /* display text directly to the OS renderer */
    virtual void print_to_os(const char *txt)
    {
        /* display the text on our OS banner window */
        os_banner_disp(banner_, txt, strlen(txt));
    }

    /* turn HTML mode on/off in the underlying OS-level renderer */
    virtual void start_html_in_os() { os_banner_start_html(banner_); }
    virtual void end_html_in_os() { os_banner_end_html(banner_); }

    /* set the text color */
    virtual void set_os_text_color(os_color_t fg, os_color_t bg)
    {
        /* set the color in the banner */
        os_banner_set_color(banner_, fg, bg);
    }

    /* set the body color */
    virtual void set_os_body_color(os_color_t color)
    {
        /* set the color in the banner */
        os_banner_set_screen_color(banner_, color);
    }

    /* set text attributes */
    virtual void set_os_text_attr(int attr)
    {
        /* set the attributes on the underlying OS-level banner */
        os_banner_set_attr(banner_, attr);
    }

    /* flush the underlying OS-level renderer */
    virtual void flush_to_os()
    {
        /* flush the OS banner window */
        os_banner_flush(banner_);
    }

    /* determine if the target supports HTML */
    virtual int get_init_html_target() const
    {
        /* if we're a text grid, the underlying window does not use HTML */
        if (win_type_ == OS_BANNER_TYPE_TEXTGRID)
            return FALSE;

        /* defer to the inherited determination in other cases */
        return CVmFormatterDisp::get_init_html_target();
    }

    /* my OS banner handle */
    void *banner_;

    /* my OS window type (OS_BANNER_TYPE_xxx) */
    int win_type_;

    /* banner style (a combination of OS_BANNER_STLE_xxx bit flags) */
    unsigned long style_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Formatter subclass for the log file 
 */
class CVmFormatterLog: public CVmFormatter
{
    friend class CVmConsole;
    friend class CVmConsoleLog;
    
public:
    CVmFormatterLog(class CVmConsole *console, int width)
        : CVmFormatter(console)
    {
        /* we have no log file yet */
        lognf_ = 0;
        logfp_ = 0;
        logglob_ = 0;

        /* remember our width */
        width_ = width;
    }

    void delete_obj(VMG0_)
    {
        /* close the log file */
        close_log_file(vmg0_);

        /* do the basic deletion */
        CVmFormatter::delete_obj(vmg0_);
    }

    /* initialize */
    void init()
    {
        /* inherit base class initialization */
        CVmFormatter::init();
        
        /* use plain text in the log file stream */
        plain_text_target_ = TRUE;
        
        /* we're not a display stream */
        is_disp_stream_ = FALSE;
        
        /* we're not an HTML formatter */
        html_target_ = FALSE;

        /* 
         *   we use our own internal line wrapping, since our underlying
         *   display layer is simply dumping to a text file 
         */
        os_line_wrap_ = FALSE;
        
        /* no log file yet */
        logfp_ = 0;
    }

    /* don't use MORE mode in a log stream */
    int formatter_more_mode() const { return FALSE; }

    /* get my width */
    virtual int get_buffer_maxcol() const { return width_; }

protected:
    /* log streams do not support HTML at the OS level */
    virtual void start_html_in_os() { }
    virtual void end_html_in_os() { }

    /* set the attributes in the underlying stream */
    virtual void set_os_text_attr(int)
    {
        /* log streams are plain text - they don't support attributes */
    }

    /* set the color in the underlying stream */
    virtual void set_os_text_color(os_color_t, os_color_t)
    {
        /* log streams are plain text - they don't support colors */
    }

    /* set the body color */
    virtual void set_os_body_color(os_color_t)
    {
        /* log streams are plain text - they don't support colors */
    }

    /* display text to the underlying OS device */
    virtual void print_to_os(const char *txt)
    {
        /* display the text through the log file */
        if (logfp_ != 0)
        {
            os_fprintz(logfp_, txt);
            osfflush(logfp_);
        }
    }

    /* flush the underlying OS-level rendere */
    virtual void flush_to_os() { }

    /* set the window title in the OS layer - no effect for log streams */
    virtual void set_title_in_os(const char *) { }

    /* open a log file */
    int open_log_file(VMG_ const char *fname);
    int open_log_file(VMG_ const vm_val_t *filespec,
                      const struct vm_rcdesc *rc);
    int open_log_file(VMG_ class CVmNetFile *netfile);

    /* set the log file to a file previously opened */
    int set_log_file(VMG_ CVmNetFile *nf, osfildef *fp);
    
    /* close the log file */
    int close_log_file(VMG0_);
    
    /* 
     *   Process a <nolog> tag.  Since we're a log stream, we hide the
     *   contents of this tag.  
     */
    virtual void process_nolog_tag(int is_end_tag)
    {
        /* turn hiding on or off as appropriate */
        if (is_end_tag)
            --html_in_ignore_;
        else
            ++html_in_ignore_;
    }

    /*
     *   Process a <log> tag.  Since we're a log stream, we show the contents
     *   of this tag, so we can simply parse and ignore it. 
     */
    virtual void process_log_tag(int /*is_end_tag*/)
    {
    }

    /* my log file handle and network file descriptor */
    osfildef *logfp_;
    class CVmNetFile *lognf_;
    struct vm_globalvar_t *logglob_;
    
    /* the maximum width to use for our lines */
    int width_;
};


#endif /* VMCONSOL_H */
