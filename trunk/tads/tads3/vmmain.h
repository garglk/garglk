/* $Header$ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmmain.h - main entrypoint to run a T3 image file
Function
  
Notes
  
Modified
  10/07/99 MJRoberts  - Creation
*/

#ifndef VMMAIN_H
#define VMMAIN_H

#include "vmglob.h"

/*
 *   Parse a command line to determine the name of the game file specified by
 *   the arguments.  If we can find a game file specification, we'll fill in
 *   'buf' with the filename and return true; if there's no file name
 *   specified, we'll return false.
 *   
 *   Note that our parsing will work for TADS 2 or TADS 3 interpreter command
 *   lines, so this routine can be used to extract the filename from an
 *   ambiguous command line in order to check the file for its type and
 *   thereby resolve which interpreter to use.
 *   
 *   Note that the filename might not come directly from the command
 *   arguments, since it might be implied.  If there's no game file directly
 *   specified, but there is an explicit "-r" option to restore a saved game,
 *   we'll pull the game filename out of the saved game file if possible.
 *   Saved game files in both TADS 2 and TADS 3 can store the original game
 *   file name that was being executed at the time the game was saved.  
 */
int vm_get_game_arg(int argc, const char *const *argv,
                    char *buf, size_t buflen);

/*
 *   Given a game file argument, determine which engine (TADS 2 or TADS 3)
 *   should be used to run the game.
 *   
 *   We'll first check to see if the given file exists.  If it does, we'll
 *   read header information from the file to try to identify the game.  If
 *   the file does not exist, we will proceed to check default suffixes, if
 *   the suffix arguments are non-null.
 *   
 *   If defexts is not null, it gives an array of default filename suffix
 *   strings, suitable for use with os_defext().  When the name as given
 *   doesn't refer to an existing file, we'll try looking for files with
 *   these suffixes, one at a time.
 *   
 *   Returns one of the VM_GGT_xxx codes.
 *   
 *   If the return value is 2 or 3, we'll fill in the actual_filename buffer
 *   with the full name of the file; if we added a default suffix, the
 *   suffix will be included in this result.  
 */
int vm_get_game_type(const char *filename,
                     char *actual_filename,
                     size_t actual_filename_buffer_length,
                     const char *const *defexts, size_t defext_count);

/*
 *   Returns codes for vm_get_game_type() 
 */

/* game type is TADS 2 */
#define VM_GGT_TADS2          2

/* game type is TADS 3 */
#define VM_GGT_TADS3          3

/* game file not found (even after trying default extensions) */
#define VM_GGT_NOT_FOUND    (-1)

/* game file exists but isn't a valid tads 2 or tads 3 game */
#define VM_GGT_INVALID      (-2)

/* 
 *   ambiguous filename - the exact filename doesn't exist, and more than
 *   one default suffix version exists 
 */
#define VM_GGT_AMBIG        (-3)

/* 
 *   determine if a VM_GGT_xxx code refers to a valid engine version:
 *   returns true if the code is an engine version, false if the code is an
 *   error indication 
 */
#define vm_ggt_is_valid(code) ((code) > 0)


/*
 *   Execute an image file.  We'll return zero on success, or a VM error code
 *   on failure.  If an error occurs, we'll fill in 'errbuf' with the text of
 *   a message describing the problem.
 *   
 *   If 'load_from_exe' is true, the image filename given is actually the
 *   name of the native executable file that we're running, and we should
 *   load the image file that's attached to the native executable file via
 *   the system-specific os_exeseek() mechanism.
 *   
 *   If 'script_file' is not null, we'll read console input from the given
 *   file.  If 'log_file' is not null, we'll log console output to the given
 *   file.  If 'cmd_log_file' is not null, we'll log each line we read from
 *   the console to the given command logging file.  
 *   
 *   'charset' optionally selects a character set to use for text displayed
 *   to or read from the user interface.  If this is null, we'll use the
 *   current system character set as indicated by the osifc layer.  'charset'
 *   should usually be null unless explicitly specified by the user.  
 */
int vm_run_image(class CVmMainClientIfc *clientifc,
                 const char *image_file_name,
                 class CVmHostIfc *hostifc,
                 const char *const *prog_argv, int prog_argc,
                 const char *script_file, int script_quiet,
                 const char *log_file, const char *cmd_log_file,
                 int load_from_exe, int show_banner,
                 const char *charset, const char *log_charset,
                 const char *saved_state, const char *res_dir);

/*
 *   Execute an image file using argc/argv conventions.  We'll parse the
 *   command line and invoke the program.
 *   
 *   The 'executable_name' is the name of the host program; this is used to
 *   prepare "usage" messages.
 *   
 *   If 'defext' is true, we'll try adding a default extension ("t3",
 *   formerly "t3x") to the name of the image file we find if the given
 *   filename doesn't exist.  We'll always check to see if the file exists
 *   with the exact given name before we do this, so that we don't add an
 *   extension where none is needed.  If the caller doesn't want us to try
 *   adding an extension at all, pass in 'defext' as false.
 *   
 *   If 'test_mode' is true, we'll make some small changes to the program
 *   invocation protocol appropriate to running system tests.  In
 *   particular, we'll build the program argument list with only the root
 *   name of the image file, not the full path - this allows the program to
 *   display the argument list without any dependencies on local path name
 *   conventions or the local directory structure, allowing for more easily
 *   portable test scripts.
 *   
 *   If 'hostifc' is null, we'll provide our own default interface.  The
 *   caller can provide a custom host interface by passing in a non-null
 *   'hostifc' value.  
 */
int vm_run_image_main(class CVmMainClientIfc *clientifc,
                      const char *executable_name,
                      int argc, char **argv, int defext, int test_mode,
                      class CVmHostIfc *hostifc);

/*
 *   VM Main client services interface.  Callers of the vm_run_image
 *   functions must provide an implementation of this interface. 
 */
class CVmMainClientIfc
{
public:
    /* 
     *   Set "plain" mode.  This should set the console to plain ASCII output
     *   mode, if appropriate.  Note that this can be called before
     *   client_init(), and no globals are generally present at this point.
     *   
     *   In most cases, this can make a call to os_plain() to set the
     *   OS-level console to plain mode.  Non-console applications generally
     *   need not do anything here at all.  
     */
    virtual void set_plain_mode() = 0;

    /*
     *   Create the main system console, if desired.  This is called during
     *   VM initialization, so it is called prior to client_init().  Returns
     *   the main console object, if desired.  If no main console is desired
     *   for this application, return null.  
     */
    virtual class CVmConsoleMain *create_console(
        struct vm_globals *globals) = 0;

    /*
     *   Delete the console, if we created one.  This is called during VM
     *   termination, so it's called after client_terminate().  If
     *   create_console() doesn't create a console, this routine need do
     *   nothing.  
     */
    virtual void delete_console(struct vm_globals *globals,
                                class CVmConsoleMain *console) = 0;

    /* 
     *   Initialization - we'll invoke this immediately after initializing
     *   the VM (via vm_initialize), so the client can perform any global
     *   initialization desired.  The globals are valid at this point because
     *   we have completed VM initialization.
     *   
     *   If script_file is non-null, it gives the name of a file to use as
     *   the source of console input.  The client implementation should set
     *   up accordingly; if the standard console (G_console) is being used,
     *   the client can simply use G_console->open_script_file() to set up
     *   scripting.
     *   
     *   If log_file is non-null, it gives the name of a file to use to log
     *   console output.  The client shoudl set up logging; if the standard
     *   console if being used, G_console->open_log_file() will do the trick.
     *   
     *   If cmd_log_file is non-null, it gives the name of a file to use to
     *   log commands read from the input (i.e., only command input should be
     *   logged, not other console output).  If the standard console is being
     *   used, G_console->open_command_log() will set things up properly.
     *   
     *   If banner_str is non-null, it gives a VM banner string that should
     *   be displayed to the user.  If the standard console is being used,
     *   this can be displayed using G_console->format_text().
     *   
     *   The parameters (script_file, log_file, cmd_log_file, and the
     *   presence or absence of banner_str) are taken from the startup
     *   parameters.  For a command-line version, for example, these come
     *   from command line options.  So, these are necessarily passed down in
     *   some form from the client to begin with; so a client that never
     *   passes these to vm_run_image() or vm_run_image_main() doesn't need
     *   to handle these parameters at all here.  
     */
    virtual void client_init(struct vm_globals *globals,
                             const char *script_file, int script_quiet,
                             const char *log_file,
                             const char *cmd_log_file,
                             const char *banner_str) = 0;

    /*
     *   Termination - we'll invoke this immediately before terminating the
     *   VM (via vm_terminate).  Globals are still valid at this point, but
     *   will be destroyed after this returns. 
     */
    virtual void client_terminate(struct vm_globals *globals) = 0;
    
    /* 
     *   pre-execution notification - we'll invoke this function just before
     *   starting execution in the loaded image 
     */
    virtual void pre_exec(struct vm_globals *globals) = 0;

    /* 
     *   terminate - we'll invoke this just after execution in the loaded
     *   image terminates 
     */
    virtual void post_exec(struct vm_globals *globals) = 0;

    /* 
     *   Terminate with error - we'll invoke this upon catching an
     *   exception that the image file doesn't handle and which thus
     *   terminates execution.  Note that if this is called, post_exec()
     *   will not be called; however, if post_exec() itself throws an
     *   exception, we'll invoke this routine.  
     */
    virtual void post_exec_err(struct vm_globals *globals) = 0;

    /* 
     *   Display an error message.  We'll call this with a complete error
     *   message to display.  Note that we won't add a newline at the end of
     *   the message, so if the message is to be displayed on a stdio-style
     *   terminal, this routine should display a newline after the message.
     *   
     *   If the implementation normally writes the text to the main output
     *   console (G_console), it must take into the account the possibility
     *   that we have not opened a system console at all (i.e., G_console
     *   could be null), or have not allocated any globals at all (i.e.,
     *   'globals' could be null).
     *   
     *   If 'add_blank_line' is true, the implementation should add a blank
     *   line after the error, if appropriate for the display device.  If
     *   we're displaying the message in an alert box on a GUI, for example,
     *   this can be ignored.  
     */
    virtual void display_error(struct vm_globals *globals,
                               const char *msg, int add_blank_line) = 0;
};

#endif /* VMMAIN_H */

